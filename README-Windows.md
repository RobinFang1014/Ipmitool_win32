# ipmitool native Windows port

This folder contains a Visual Studio 2026 native Windows build of ipmitool.

## Attribution / 專案來源

This project is based on ipmitool:
https://github.com/ipmitool/ipmitool

Original ipmitool source code is distributed under the license in `COPYING`.
Please keep `COPYING`, `AUTHORS`, and the original project documentation when redistributing this repository or binary releases.

本專案的原始碼來自 ipmitool：
https://github.com/ipmitool/ipmitool

原始 ipmitool 程式碼依照 `COPYING` 內的授權條款散布。
重新散布此 repository 或 binary release 時，請保留 `COPYING`、`AUTHORS` 與原專案文件。

## Windows Port / Windows 移植說明

This repository contains a native Windows / Visual Studio port of ipmitool.
This is not an official ipmitool release.

本 repository 包含 ipmitool 的 native Windows / Visual Studio 移植版本。
此版本不是官方 ipmitool release。

## AI-Assisted Work Division / AI 協作分工

This Windows port was assisted by AI tools, including OpenAI Codex, Google Gemini, and Anthropic Claude.

本 Windows 移植版本有 AI 工具協助，包含 OpenAI Codex、Google Gemini 與 Anthropic Claude。

### OpenAI Codex

- Assisted in converting the Linux-oriented ipmitool source code into a Visual Studio native Windows project.

### Google Gemini

- Assisted in adding the local `enterprise-numbers.txt` file.
- Assisted in converting the `enterprise-numbers.txt` loading code so the Windows build can read it as an embedded resource.
- Assisted in implementing or adapting the WMI interface for native Windows IPMI access.

### Anthropic Claude

- Assisted in fixing `sel time set` applying the time zone offset twice, which made the BMC clock 8 or 16 hours off.
- Assisted in replacing the long Windows time zone name in time output with a short abbreviation (e.g. `TST`, or `UTC` with `-Z`).
- Assisted in fixing `WORDS_BIGENDIAN` being defined as 0, which made the x64 build use the big-endian bit-field layouts.
- Assisted in packing the DCMI / Node Manager structures on MSVC (wrong `dcmi power reading` values; `nm policy add` sent shifted data).
- Assisted in opening binary files in binary mode (`sdr dump`, `sdr fill file`, `-S`, `sel writeraw` / `readraw`, `fru internaluse`, `gendev`, `ekanalyzer`).
- Assisted in fixing struct layouts that differed from GCC: `#pragma pack(0)` is ignored by MSVC, and the Dell LOM MAC record (`delloem mac`) had the wrong size. All structs were compared between MSVC and GCC.
- Assisted in making Ctrl+C close the BMC session on the main thread instead of a second thread.
- Assisted in showing the real reason for network errors, and a "No response from <host>" hint when the BMC does not answer.
- Assisted in removing compatibility macros that renamed struct members (e.g. `intf->close`).
- Assisted in fixing a `pef` crash on 64-bit Windows and clearing all compiler warnings.
- Assisted in fixing WMI error handling: failures are reported instead of being returned as empty successful responses, bridged requests are refused instead of being sent to the BMC, and the reason is shown when WMI cannot be opened.
- Assisted in removing the CR left at the end of IANA vendor names.
- Assisted in sending F1–F12 and turning on VT output processing in SOL.

### 中文分工

### OpenAI Codex

- 協助將原本偏 Linux 的 ipmitool source code 轉換成 Visual Studio native Windows project。

### Google Gemini

- 協助加入本地端 `enterprise-numbers.txt` 檔案。
- 協助轉換 `enterprise-numbers.txt` 的讀取程式碼，讓 Windows build 可以從內嵌 resource 讀取資料。
- 協助實作或調整 WMI interface，供 native Windows IPMI 存取使用。

### Anthropic Claude

- 協助修正 `sel time set` 重複套用時區偏移，導致 BMC 時間差 8 或 16 小時的問題。
- 協助將時間顯示中過長的 Windows 時區名稱改為縮寫（例如 `TST`；加 `-Z` 時顯示 `UTC`）。
- 協助修正 `WORDS_BIGENDIAN` 被定義成 0，導致 x64 版使用 big-endian bit-field 排列的問題。
- 協助在 MSVC 上 pack DCMI / Node Manager 結構（`dcmi power reading` 數值錯誤；`nm policy add` 送出錯位的資料）。
- 協助將 binary 檔改用 binary mode 開啟（`sdr dump`、`sdr fill file`、`-S`、`sel writeraw` / `readraw`、`fru internaluse`、`gendev`、`ekanalyzer`）。
- 協助修正與 GCC 不一致的 struct 排列：MSVC 會忽略 `#pragma pack(0)`，Dell LOM MAC record（`delloem mac`）大小錯誤。所有 struct 都已比對過 MSVC 與 GCC 的排列。
- 協助讓 Ctrl+C 由主執行緒關閉 BMC session，而不是在另一個執行緒處理。
- 協助在網路錯誤時顯示實際原因，並在 BMC 沒有回應時提示「No response from <host>」。
- 協助移除會把 struct 成員改名的相容性 macro（例如 `intf->close`）。
- 協助修正 64-bit Windows 上 `pef` 的 crash，並清除所有編譯器 warning。
- 協助修正 WMI 的錯誤處理：失敗時回報錯誤，不再回傳空的成功回應；拒絕 bridging 請求，不再送給 BMC；無法開啟 WMI 時說明原因。
- 協助移除 IANA 廠商名稱結尾多出的 CR。
- 協助在 SOL 中送出 F1–F12，並開啟 VT 輸出處理。

## IANA Enterprise Numbers / IANA 企業編號資料

The embedded `enterprise-numbers.txt` data comes from IANA Private Enterprise Numbers:
https://www.iana.org/assignments/enterprise-numbers/

The Windows build embeds this file as a resource so the executable can load the registry data without requiring an external text file.

內嵌的 `enterprise-numbers.txt` 資料來自 IANA Private Enterprise Numbers：
https://www.iana.org/assignments/enterprise-numbers/

Windows 版本會將此檔案內嵌成 resource，讓執行檔不需要外部文字檔也能讀取 registry data。

## Interfaces

- `wmi`: local in-band IPMI through `ROOT\WMI:Microsoft_IPMI`.
- `lan`: IPMI v1.5 RMCP over Winsock.
- `lanplus`: IPMI v2.0 RMCP+ over Winsock with CNG/BCrypt for HMAC, AES-CBC, and random bytes.

Linux in-band plugins such as `open`, `free`, `bmc`, `imb`, `lipmi`, `dbus`, `usb`, and POSIX serial plugins are intentionally not built.

## Build

Open `ipmitool-windows.sln` in Visual Studio 2026 and build `x64`. The project uses the VS 2026 `v145` MSVC platform toolset installed with this environment.

Command line:

```bat
"C:\Program Files\Microsoft Visual Studio\18\Community\MSBuild\Current\Bin\MSBuild.exe" ipmitool-windows.sln /m /p:Configuration=Release /p:Platform=x64
```

## Examples

```bat
bin\x64\Release\ipmitool.exe -I wmi mc info
bin\x64\Release\ipmitool.exe -I wmi raw 6 1
bin\x64\Release\ipmitool.exe -I lan -H 192.0.2.10 -U admin -P password mc info
bin\x64\Release\ipmitool.exe -I lanplus -H 192.0.2.10 -U admin -P password mc info
```

## Compatibility Warnings

- WMI requires the Microsoft IPMI driver/provider (`IpmiDrv.sys`) and a `Microsoft_IPMI` instance in `ROOT\WMI`, and ipmitool must run as Administrator. When WMI cannot be opened, ipmitool prints the reason.
- WMI is local in-band only and reaches only the local BMC. Bridged requests (`-t`, `-b`, `-T`, `-B`, and sensors owned by another controller such as Intel ME) fail with "WMI interface cannot bridge ..."; use `lanplus` for them, or `lan` / `lanplus` for network BMC access.
- SOL (`sol activate`) works over `lanplus` in a Windows console. Arrow keys, Home/End, PgUp/PgDn, Insert/Delete and F1–F12 are sent as xterm escape sequences, VT output processing is on during the session, and Ctrl+C is sent to the remote side. The Tyan / IPMI v1.5 `tsol` and `isol` commands, and `sunoem`, are not built.
- Ctrl+C outside SOL closes the BMC session before ipmitool exits; press Ctrl+C again to exit at once.
- Daemon mode is not supported on Windows.
- LANPLUS cryptography uses Windows CNG instead of OpenSSL.
