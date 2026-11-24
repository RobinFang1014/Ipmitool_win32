#if defined(_WIN32) || defined(__CYGWIN__)
/*
 * wmibmc.c - Windows WMI IPMI interface for ipmitool
 *
 * Sends IPMI requests to the local BMC through the Microsoft IPMI driver's
 * WMI class (ROOT\WMI, Microsoft_IPMI, method RequestResponse).
 *
 * Error handling: any failed WMI/COM call makes sendrecv() return NULL, so
 * the command prints its usual "Error sending ... command". A failed call
 * must never look like a successful response with empty data.
 */

#include <windows.h>
#include <oleauto.h>
#include <wbemidl.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include <ipmitool/ipmi.h>
#include <ipmitool/ipmi_intf.h>
#include <ipmitool/helper.h>
#include <ipmitool/log.h>

static IWbemLocator  *g_locator       = NULL;
static IWbemServices *g_services      = NULL;
static BSTR           g_instance_path = NULL;
static int            g_com_init      = 0;

static void winbmc_close(struct ipmi_intf *intf) {
    if (g_instance_path) { SysFreeString(g_instance_path); g_instance_path = NULL; }
    if (g_services) { g_services->lpVtbl->Release(g_services); g_services = NULL; }
    if (g_locator)  { g_locator->lpVtbl->Release(g_locator);   g_locator = NULL; }
    if (g_com_init) { CoUninitialize(); g_com_init = 0; }
    intf->opened = 0;
}

/* Explain why the Microsoft_IPMI class could not be used. */
static void winbmc_report_open_error(HRESULT hr, int found_instance_list)
{
    if (hr == WBEM_E_INVALID_CLASS || hr == WBEM_E_NOT_FOUND) {
        lprintf(LOG_ERR, "WMI: class Microsoft_IPMI not found (0x%08lx). "
                "The Microsoft IPMI driver is not loaded or no BMC was "
                "detected on this machine. Use -I lanplus -H <BMC address> "
                "to reach a BMC over the network.", (unsigned long)hr);
    } else if (hr == WBEM_E_ACCESS_DENIED || hr == E_ACCESSDENIED) {
        lprintf(LOG_ERR, "WMI: access to Microsoft_IPMI denied (0x%08lx). "
                "Run ipmitool as Administrator.", (unsigned long)hr);
    } else if (found_instance_list) {
        lprintf(LOG_ERR, "WMI: no Microsoft_IPMI instance: no BMC was "
                "detected by the Microsoft IPMI driver.");
    } else {
        lprintf(LOG_ERR, "WMI: cannot use Microsoft_IPMI (0x%08lx).",
                (unsigned long)hr);
    }
}

static int winbmc_open(struct ipmi_intf *intf) {
    HRESULT hr;
    BSTR ns = NULL;
    BSTR bstr_class = NULL;
    IEnumWbemClassObject *pEnum = NULL;

    if (intf->opened) return 0;

    hr = CoInitializeEx(NULL, COINIT_MULTITHREADED);
    if (SUCCEEDED(hr)) g_com_init = 1;

    hr = CoCreateInstance(&CLSID_WbemLocator, 0, CLSCTX_INPROC_SERVER, &IID_IWbemLocator, (void**)&g_locator);
    if (FAILED(hr) || !g_locator) {
        lprintf(LOG_ERR, "WMI: cannot create WbemLocator (0x%08lx)", (unsigned long)hr);
        goto fail;
    }

    ns = SysAllocString(L"ROOT\\WMI");
    if (!ns) goto fail;
    hr = g_locator->lpVtbl->ConnectServer(g_locator, ns, NULL, NULL, NULL, 0, NULL, NULL, &g_services);
    SysFreeString(ns);
    if (FAILED(hr) || !g_services) {
        lprintf(LOG_ERR, "WMI: cannot connect to ROOT\\WMI (0x%08lx)", (unsigned long)hr);
        goto fail;
    }

    CoSetProxyBlanket((IUnknown*)g_services, RPC_C_AUTHN_WINNT, RPC_C_AUTHZ_NONE, NULL,
                      RPC_C_AUTHN_LEVEL_CALL, RPC_C_IMP_LEVEL_IMPERSONATE, NULL, EOAC_NONE);

    /* Find the instance path of the (single) Microsoft_IPMI object */
    bstr_class = SysAllocString(L"Microsoft_IPMI");
    if (!bstr_class) goto fail;
    hr = g_services->lpVtbl->CreateInstanceEnum(g_services, bstr_class, 0, NULL, &pEnum);
    SysFreeString(bstr_class);

    if (SUCCEEDED(hr) && pEnum) {
        IWbemClassObject *pInst = NULL;
        ULONG uRet = 0;
        if (pEnum->lpVtbl->Next(pEnum, WBEM_INFINITE, 1, &pInst, &uRet) == S_OK && uRet > 0 && pInst) {
            VARIANT v; VariantInit(&v);
            if (SUCCEEDED(pInst->lpVtbl->Get(pInst, L"__PATH", 0, &v, NULL, NULL)) &&
                V_VT(&v) == VT_BSTR) {
                g_instance_path = SysAllocString(V_BSTR(&v));
            }
            VariantClear(&v);
            pInst->lpVtbl->Release(pInst);
        }
        pEnum->lpVtbl->Release(pEnum);
    }

    if (!g_instance_path) {
        winbmc_report_open_error(hr, SUCCEEDED(hr));
        goto fail;
    }

    intf->opened = 1;
    return 0;

fail:
    winbmc_close(intf);
    return -1;
}

/* Put a VT_UI1 property on the method's input object */
static int winbmc_put_u1(IWbemClassObject *obj, LPCWSTR name, BYTE val)
{
    VARIANT v;
    HRESULT hr;

    VariantInit(&v);
    v.vt = VT_UI1;
    v.bVal = val;
    hr = obj->lpVtbl->Put(obj, name, 0, &v, 0);
    if (FAILED(hr))
        lprintf(LOG_DEBUG, "WMI: Put(%ls) failed (0x%08lx)", name, (unsigned long)hr);
    return SUCCEEDED(hr);
}

static struct ipmi_rs *winbmc_sendrecv(struct ipmi_intf *intf, struct ipmi_rq *req) {
    static struct ipmi_rs rsp;
    static int bridge_reported = 0;
    struct ipmi_rs *result = NULL;
    HRESULT hr;
    IWbemClassObject *pClass = NULL, *pInDef = NULL, *pInInst = NULL, *pOutInst = NULL;
    BSTR bstr_class = NULL;
    BSTR bstr_meth = NULL;
    VARIANT v, vr;
    uint8_t our_addr;

    /* Ctrl+C: send no more requests, so command loops end quickly */
    if (ipmi_is_interrupted()) return NULL;
    if (!intf->opened && winbmc_open(intf) < 0) return NULL;

    /*
     * The Microsoft IPMI driver only talks to the local BMC. Requests for
     * another controller (bridging: sensors owned by e.g. Intel ME at 0x2C,
     * or -t/-b/-T/-B) used to be sent to the BMC instead, which returned
     * the BMC's answer for the wrong device. Same rule as lan/lanplus:
     * bridging is needed when target_addr differs from our own address.
     */
    our_addr = intf->my_addr ? intf->my_addr : IPMI_BMC_SLAVE_ADDR;
    if (intf->target_addr != 0 && intf->target_addr != our_addr) {
        lprintf(bridge_reported ? LOG_DEBUG : LOG_ERR,
                "WMI interface cannot bridge to IPMB address 0x%02x "
                "(channel %d); use -I lanplus for this.",
                intf->target_addr, intf->target_channel);
        bridge_reported = 1;
        return NULL;
    }

    memset(&rsp, 0, sizeof(struct ipmi_rs));
    VariantInit(&v);
    VariantInit(&vr);

    bstr_class = SysAllocString(L"Microsoft_IPMI");
    bstr_meth = SysAllocString(L"RequestResponse");
    if (!bstr_class || !bstr_meth) goto out;

    /* Get the method's input parameter object */
    hr = g_services->lpVtbl->GetObject(g_services, bstr_class, 0, NULL, &pClass, NULL);
    if (FAILED(hr) || !pClass) {
        lprintf(LOG_DEBUG, "WMI: GetObject(Microsoft_IPMI) failed (0x%08lx)", (unsigned long)hr);
        goto out;
    }
    hr = pClass->lpVtbl->GetMethod(pClass, bstr_meth, 0, &pInDef, NULL);
    if (FAILED(hr) || !pInDef) {
        lprintf(LOG_DEBUG, "WMI: GetMethod(RequestResponse) failed (0x%08lx)", (unsigned long)hr);
        goto out;
    }
    hr = pInDef->lpVtbl->SpawnInstance(pInDef, 0, &pInInst);
    if (FAILED(hr) || !pInInst) {
        lprintf(LOG_DEBUG, "WMI: SpawnInstance failed (0x%08lx)", (unsigned long)hr);
        goto out;
    }

    /* Command, Lun and NetworkFunction as VT_UI1 */
    if (!winbmc_put_u1(pInInst, L"Command", (BYTE)req->msg.cmd) ||
        !winbmc_put_u1(pInInst, L"Lun", (BYTE)(req->msg.lun & 0x3)) ||
        !winbmc_put_u1(pInInst, L"NetworkFunction", (BYTE)req->msg.netfn))
        goto out;

    /* RequestData: byte array, or VT_NULL when there is no data */
    if (req->msg.data_len > 0) {
        SAFEARRAYBOUND bound = { (ULONG)req->msg.data_len, 0 };
        SAFEARRAY *sa = SafeArrayCreate(VT_UI1, 1, &bound);
        void *pDest = NULL;

        if (!sa) goto out;
        if (FAILED(SafeArrayAccessData(sa, &pDest))) {
            SafeArrayDestroy(sa);
            goto out;
        }
        memcpy(pDest, req->msg.data, req->msg.data_len);
        SafeArrayUnaccessData(sa);
        v.vt = VT_ARRAY | VT_UI1;
        v.parray = sa;          /* freed by VariantClear() below */
    } else {
        v.vt = VT_NULL;
    }
    hr = pInInst->lpVtbl->Put(pInInst, L"RequestData", 0, &v, 0);
    VariantClear(&v);           /* Put() made its own copy */
    if (FAILED(hr)) {
        lprintf(LOG_DEBUG, "WMI: Put(RequestData) failed (0x%08lx)", (unsigned long)hr);
        goto out;
    }

    /* RequestDataSize as VT_I4 (some drivers do not accept UI4) */
    v.vt = VT_I4;
    v.lVal = (long)req->msg.data_len;
    hr = pInInst->lpVtbl->Put(pInInst, L"RequestDataSize", 0, &v, 0);
    VariantClear(&v);
    if (FAILED(hr)) {
        lprintf(LOG_DEBUG, "WMI: Put(RequestDataSize) failed (0x%08lx)", (unsigned long)hr);
        goto out;
    }

    /* ResponderAddress: the local BMC */
    if (!winbmc_put_u1(pInInst, L"ResponderAddress", IPMI_BMC_SLAVE_ADDR))
        goto out;

    /* --- run the method --- */
    hr = g_services->lpVtbl->ExecMethod(g_services, g_instance_path, bstr_meth, 0, NULL, pInInst, &pOutInst, NULL);
    if (FAILED(hr) || !pOutInst) {
        lprintf(LOG_DEBUG, "WMI: RequestResponse failed (0x%08lx), netfn 0x%02x cmd 0x%02x, %d data bytes",
                (unsigned long)hr, req->msg.netfn, req->msg.cmd, (int)req->msg.data_len);
        goto out;
    }

    /* CompletionCode is required: without it there is no valid response */
    hr = pOutInst->lpVtbl->Get(pOutInst, L"CompletionCode", 0, &vr, NULL, NULL);
    if (FAILED(hr) || FAILED(VariantChangeType(&vr, &vr, 0, VT_UI4))) {
        lprintf(LOG_DEBUG, "WMI: RequestResponse returned no CompletionCode (0x%08lx)", (unsigned long)hr);
        VariantClear(&vr);
        goto out;
    }
    rsp.ccode = (uint8_t)vr.ulVal;
    VariantClear(&vr);

    /* ResponseData: the first byte repeats the completion code */
    hr = pOutInst->lpVtbl->Get(pOutInst, L"ResponseData", 0, &vr, NULL, NULL);
    if (SUCCEEDED(hr) && (vr.vt & VT_ARRAY) && vr.parray &&
        SafeArrayGetDim(vr.parray) == 1 && SafeArrayGetElemsize(vr.parray) == 1) {
        LONG lb = 0, ub = -1;
        uint8_t *raw = NULL;

        if (SUCCEEDED(SafeArrayGetLBound(vr.parray, 1, &lb)) &&
            SUCCEEDED(SafeArrayGetUBound(vr.parray, 1, &ub)) &&
            ub - lb + 1 > 1 &&
            SUCCEEDED(SafeArrayAccessData(vr.parray, (void **)&raw))) {
            int n = (int)(ub - lb + 1) - 1;   /* skip the completion code */

            if (n > (int)sizeof(rsp.data))
                n = (int)sizeof(rsp.data);
            memcpy(rsp.data, raw + 1, n);
            rsp.data_len = n;
            SafeArrayUnaccessData(vr.parray);
        }
    }
    VariantClear(&vr);

    result = &rsp;

out:
    if (pOutInst) pOutInst->lpVtbl->Release(pOutInst);
    if (pInInst) pInInst->lpVtbl->Release(pInInst);
    if (pInDef) pInDef->lpVtbl->Release(pInDef);
    if (pClass) pClass->lpVtbl->Release(pClass);
    if (bstr_meth) SysFreeString(bstr_meth);
    if (bstr_class) SysFreeString(bstr_class);
    return result;
}

struct ipmi_intf ipmi_wmi_intf = {
    .name = "wmi",
    .desc = "Windows WMI IPMI Interface",
    .open = winbmc_open,
    .close = winbmc_close,
    .sendrecv = winbmc_sendrecv,
};
#endif
