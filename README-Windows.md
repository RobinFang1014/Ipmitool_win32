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

This Windows port was assisted by AI tools, including OpenAI Codex and Google Gemini.

本 Windows 移植版本有 AI 工具協助，包含 OpenAI Codex 與 Google Gemini。

### OpenAI Codex

- Assisted in converting the Linux-oriented ipmitool source code into a Visual Studio native Windows project.

### Google Gemini

- Assisted in adding the local `enterprise-numbers.txt` file.
- Assisted in converting the `enterprise-numbers.txt` loading code so the Windows build can read it as an embedded resource.
- Assisted in implementing or adapting the WMI interface for native Windows IPMI access.

### 中文分工

### OpenAI Codex

- 協助將原本偏 Linux 的 ipmitool source code 轉換成 Visual Studio native Windows project。

### Google Gemini

- 協助加入本地端 `enterprise-numbers.txt` 檔案。
- 協助轉換 `enterprise-numbers.txt` 的讀取程式碼，讓 Windows build 可以從內嵌 resource 讀取資料。
- 協助實作或調整 WMI interface，供 native Windows IPMI 存取使用。

## IANA Enterprise Numbers / IANA 企業編號資料

The embedded `win32/enterprise-numbers.txt` data comes from IANA Private Enterprise Numbers:
https://www.iana.org/assignments/enterprise-numbers/

The Windows build embeds this file as a resource so the executable can load the registry data without requiring an external text file.

內嵌的 `win32/enterprise-numbers.txt` 資料來自 IANA Private Enterprise Numbers：
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

- WMI requires the Microsoft IPMI driver/provider (`IpmiDrv.sys`) and a `Microsoft_IPMI` instance in `ROOT\WMI`.
- WMI is local in-band only; use `lan` or `lanplus` for network BMC access.
- Interactive SOL/TSOL/ISOL terminal commands are not enabled in this native Windows project yet because the original code depends on POSIX `termios`, `poll`, and stdin file-descriptor semantics.
- LANPLUS cryptography uses Windows CNG instead of OpenSSL.
