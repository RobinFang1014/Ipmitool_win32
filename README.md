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

## Rights Concerns / 權利疑慮

This repository is intended to preserve the original ipmitool license and attribution.
If you believe any content in this repository violates your rights or has incorrect attribution, please open a GitHub Issue with details. I will review the concern promptly and remove, replace, or correct the affected content as appropriate.

本 repository 旨在保留原始 ipmitool 的授權與來源標註。
如果您認為此 repository 中有任何內容涉及權利疑慮或來源標註不正確，請透過 GitHub Issue 提供詳細資訊。我會盡快確認，並視情況移除、替換或修正相關內容。
