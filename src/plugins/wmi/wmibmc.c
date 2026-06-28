#if defined(_WIN32) || defined(__CYGWIN__)
/*
 * winbmc.c - Windows WMI IPMI Interface for ipmitool
 * Integrated version with automatic CC offset detection and correct parameter ordering.
 */

#include <windows.h>
#include <oleauto.h>
#include <wbemidl.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include <ipmitool/ipmi.h>
#include <ipmitool/ipmi_intf.h>

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

static int winbmc_open(struct ipmi_intf *intf) {
    HRESULT hr;
    if (intf->opened) return 0;

    hr = CoInitializeEx(NULL, COINIT_MULTITHREADED);
    if (SUCCEEDED(hr)) g_com_init = 1;

    hr = CoCreateInstance(&CLSID_WbemLocator, 0, CLSCTX_INPROC_SERVER, &IID_IWbemLocator, (void**)&g_locator);
    if (FAILED(hr)) return -1;

    BSTR ns = SysAllocString(L"ROOT\\WMI");
    hr = g_locator->lpVtbl->ConnectServer(g_locator, ns, NULL, NULL, NULL, 0, NULL, NULL, &g_services);
    SysFreeString(ns);
    if (FAILED(hr)) return -1;

    CoSetProxyBlanket((IUnknown*)g_services, RPC_C_AUTHN_WINNT, RPC_C_AUTHZ_NONE, NULL,
                      RPC_C_AUTHN_LEVEL_CALL, RPC_C_IMP_LEVEL_IMPERSONATE, NULL, EOAC_NONE);

    // 尋找實例路徑 (Instance Path)
    IEnumWbemClassObject* pEnum = NULL;
    BSTR bstr_class = SysAllocString(L"Microsoft_IPMI");
    hr = g_services->lpVtbl->CreateInstanceEnum(g_services, bstr_class, 0, NULL, &pEnum);
    SysFreeString(bstr_class);

    if (SUCCEEDED(hr)) {
        IWbemClassObject *pInst = NULL;
        ULONG uRet = 0;
        if (pEnum->lpVtbl->Next(pEnum, WBEM_INFINITE, 1, &pInst, &uRet) == S_OK && uRet > 0) {
            VARIANT v; VariantInit(&v);
            if (SUCCEEDED(pInst->lpVtbl->Get(pInst, L"__PATH", 0, &v, NULL, NULL))) {
                g_instance_path = SysAllocString(V_BSTR(&v));
                VariantClear(&v);
            }
            pInst->lpVtbl->Release(pInst);
        }
        pEnum->lpVtbl->Release(pEnum);
    }

    if (!g_instance_path) {
        winbmc_close(intf);
        return -1;
    }

    intf->opened = 1;
    return 0;
}

static struct ipmi_rs *winbmc_sendrecv(struct ipmi_intf *intf, struct ipmi_rq *req) {
static struct ipmi_rs rsp;
    HRESULT hr;
    IWbemClassObject *pClass = NULL, *pInDef = NULL, *pInInst = NULL, *pOutInst = NULL;
    BSTR bstr_meth = SysAllocString(L"RequestResponse");
    VARIANT v;

    if (!intf->opened && winbmc_open(intf) < 0) return NULL;
    memset(&rsp, 0, sizeof(struct ipmi_rs));

    // 取得類別與實例
    g_services->lpVtbl->GetObject(g_services, SysAllocString(L"Microsoft_IPMI"), 0, NULL, &pClass, NULL);
    pClass->lpVtbl->GetMethod(pClass, bstr_meth, 0, &pInDef, NULL);
    pInDef->lpVtbl->SpawnInstance(pInDef, 0, &pInInst);
    // 1. Command & NetFn (強行轉為 VT_UI1)
    VariantInit(&v); v.vt = VT_UI1; v.bVal = (BYTE)req->msg.cmd;
    pInInst->lpVtbl->Put(pInInst, L"Command", 0, &v, 0);

    v.bVal = 0;
    pInInst->lpVtbl->Put(pInInst, L"Lun", 0, &v, 0);

    v.bVal = (BYTE)req->msg.netfn;
    pInInst->lpVtbl->Put(pInInst, L"NetworkFunction", 0, &v, 0);

    // 2. RequestData - 這裡最關鍵
    if (req->msg.data_len > 0) {
        SAFEARRAYBOUND bound = { (ULONG)req->msg.data_len, 0 };
        SAFEARRAY *sa = SafeArrayCreate(VT_UI1, 1, &bound);
        void* pDest;
        SafeArrayAccessData(sa, &pDest);
        memcpy(pDest, req->msg.data, req->msg.data_len);
        SafeArrayUnaccessData(sa);
        VariantInit(&v); v.vt = VT_ARRAY | VT_UI1; v.parray = sa;
        pInInst->lpVtbl->Put(pInInst, L"RequestData", 0, &v, 0);
    } else {
        // 如果長度為 0，改用 VT_NULL 而不是空 SAFEARRAY
        VariantInit(&v); v.vt = VT_NULL;
        pInInst->lpVtbl->Put(pInInst, L"RequestData", 0, &v, 0);
    }

    // 3. RequestDataSize (強行轉為 VT_I4，有時驅動不吃 UI4)
    VariantInit(&v); v.vt = VT_I4; v.lVal = (long)req->msg.data_len;
    pInInst->lpVtbl->Put(pInInst, L"RequestDataSize", 0, &v, 0);

    // 4. ResponderAddress (強行 0x20)
    VariantInit(&v); v.vt = VT_UI1; v.bVal = 0x20;
    pInInst->lpVtbl->Put(pInInst, L"ResponderAddress", 0, &v, 0);

    // --- 執行方法 ---
    hr = g_services->lpVtbl->ExecMethod(g_services, g_instance_path, bstr_meth, 0, NULL, pInInst, &pOutInst, NULL);

    if (FAILED(hr)) {
        printf("[DEBUG] Exec Failed: 0x%08lx (N:0x%x C:0x%x Size:%d)\n", 
               (unsigned long)hr, req->msg.netfn, req->msg.cmd, (int)req->msg.data_len);
    } else if (pOutInst) {
        VARIANT vr; VariantInit(&vr);
        ULONG prop_size = 0;

        // 取得 ResponseDataSize 作為參考
        if (SUCCEEDED(pOutInst->lpVtbl->Get(pOutInst, L"ResponseDataSize", 0, &vr, NULL, NULL))) {
            prop_size = (vr.vt == VT_I4 || vr.vt == VT_UI4) ? vr.ulVal : vr.lVal;
        }
        VariantClear(&vr);

        // 取得 CompletionCode
        if (SUCCEEDED(pOutInst->lpVtbl->Get(pOutInst, L"CompletionCode", 0, &vr, NULL, NULL))) {
            rsp.ccode = (uint8_t)((vr.vt == VT_UI1) ? vr.bVal : vr.lVal);
        }
        VariantClear(&vr);

        // 取得 ResponseData (加入 Command 判斷邏輯)
        if (SUCCEEDED(pOutInst->lpVtbl->Get(pOutInst, L"ResponseData", 0, &vr, NULL, NULL)) && (vr.vt & VT_ARRAY)) {
            LONG l, u;
            SafeArrayGetLBound(vr.parray, 1, &l);
            SafeArrayGetUBound(vr.parray, 1, &u);
            int n = (int)(u - l + 1);
            if (n > 0) {
                 uint8_t* pRaw;
                 SafeArrayAccessData(vr.parray, (void**)&pRaw);
                 
                 int offset = 1; // 只跳過第一個 00 (Completion Code)
 
                 rsp.data_len = n - offset;
                 if (rsp.data_len > (int)sizeof(rsp.data)) rsp.data_len = sizeof(rsp.data);
                 if (rsp.data_len > 0) memcpy(rsp.data, pRaw + offset, rsp.data_len);
                 
                 SafeArrayAccessData(vr.parray, (void**)&pRaw);
                 SafeArrayUnaccessData(vr.parray);
             }
        }
        VariantClear(&vr);
    }
    if (pInInst) pInInst->lpVtbl->Release(pInInst);
    if (pInDef) pInDef->lpVtbl->Release(pInDef);
    if (pOutInst) pOutInst->lpVtbl->Release(pOutInst);
    if (pClass) pClass->lpVtbl->Release(pClass);
    return &rsp;
}

struct ipmi_intf ipmi_wmi_intf = {
    .name = "wmi",
    .desc = "Windows WMI IPMI Interface",
    .open = winbmc_open,
    .close = winbmc_close,
    .sendrecv = winbmc_sendrecv,
};
#endif