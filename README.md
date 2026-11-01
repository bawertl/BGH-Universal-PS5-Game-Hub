
<p align="center">
  <img src="https://github.com/user-attachments/assets/38fcf410-3211-43dd-bf1c-9b7f2c51a994" alt="BGH Logo" width="380">
</p>

<h1 align="center">BGH – BawertL's Game Hub</h1>

<p align="center">
  <strong>One Game Hub. Multiple Consoles. One Library.</strong>
</p>

<p align="center">
  Universal Game Library & Emulator Launcher<br>
  for jailbroken PlayStation 5 consoles.
</p>

<p align="center">
  <a href="https://github.com/bawertl/BGH-Universal-PS5-Game-Hub/releases/tag/v1.1.32"><strong>🎮 Download BGH V1.1.32</strong></a>
  &nbsp; | &nbsp;
  <a href="https://ko-fi.com/bawertl"><strong>💙 Support Development</strong></a>
</p>

---

## 🎮 Interface Preview

<p align="center">
  <img src="https://github.com/user-attachments/assets/a60c1c4f-42e6-41ba-ac28-1c14e3aa1447" alt="BGH Game Library" width="850">
</p>

*BGH interface preview. Appearance may vary depending on the selected theme and settings.*

---

## ✨ Features

- **12 configurable console and platform profiles**
- **6 platforms with confirmed game-launch functionality in the tested setup**
- **PlayStation 5 and PlayStation 4 game library**
- **PlayStation 2 integration with PS5SX2**
- **PlayStation 3 integration with RPCS3-5**
- **Nintendo Switch integration with ProsperoEden and ForgeNX**
- **Xbox 360 integration with PS5X360**
- Additional library profiles for PS1, PSP, Original Xbox, Dreamcast, GameCube and Wii
- Console icons and customizable platform sorting
- Configurable game directories
- Game library scanning and cover artwork
- Favorites and recently played games
- Customizable backgrounds, logos and appearance
- Controller-friendly navigation
- German and English language support
- Emulator detection and launch profiles

**IMPORTANT: BGH is a game launcher and library manager, NOT an emulator.**

All external emulators must be installed separately.

---

## 🎮 Supported Platforms

**BGH V1.1.32 includes 12 built-in platform profiles.**

Each profile has its own console icon, configurable game folders and prepared game-file scanning support.

### ✅ Confirmed Game-Launch Platforms

The following platforms have working game-launch functionality in the developer's tested PS5 setup:

| Platform | Integration | Status |
|---|---|---|
| **PlayStation 2** | PS5SX2 | ✅ Working |
| **PlayStation 3** | RPCS3-5 | ✅ Working |
| **PlayStation 4** | PS4 game launcher | ✅ Working |
| **PlayStation 5** | PS5 game launcher | ✅ Working |
| **Nintendo Switch** | ProsperoEden / ForgeNX | ✅ Working |
| **Xbox 360** | PS5X360 | ✅ Working |

**PlayStation 3 direct launching through BGH has been confirmed by the developer in the tested setup.**

This does not guarantee compatibility with every PS3 game or every emulator version.

### 🧩 Additional Platform Profiles

BGH also includes six additional console profiles:

| Platform | Library Profile | Direct Launch |
|---|---|---|
| PlayStation 1 | ✅ Prepared | Not implemented |
| PlayStation Portable (PSP) | ✅ Prepared | Not implemented |
| Original Xbox | ✅ Prepared | Not implemented |
| Sega Dreamcast | ✅ Prepared | Not implemented |
| Nintendo GameCube | ✅ Prepared | Not implemented |
| Nintendo Wii | ✅ Prepared | Not implemented |

These additional profiles include:

- Console icons
- Configurable game folders
- Prepared file-extension scanning
- Platform visibility settings
- Custom platform sorting
- Library organization

**IMPORTANT:**

Additional platform profiles can be enabled through the BGH settings.

**Enabling a platform does NOT automatically provide a working emulator or direct game-launch backend.**

Actual functionality depends on compatible software, emulator support and configured game paths.

---

## 📦 Latest Release

### BGH V1.1.32

**Tested environment:**

- **Console:** PlayStation 5
- **Firmware:** 13.60
- **Homebrew:** etaHEN
- **Application ID:** `PPSA99079`

### ⬇️ Download

**[DOWNLOAD BGH V1.1.32 AND REQUIRED FILES](https://github.com/bawertl/BGH-Universal-PS5-Game-Hub/releases/tag/v1.1.32)**

The release provides the PS5 application ZIP and supporting ELF files.

Complete installation instructions are available in the release notes.

---

## 🔧 Requirements

The tested BGH configuration uses:

| Component | Purpose |
|---|---|
| **etaHEN** | Jailbreak and homebrew environment |
| **ShadowMountPlus** | Supported PS5 dump and USB functionality |
| **Launcher / Forwarder ELF** | Game and emulator launch requests |
| **ftpsrv** | FTP-based library functionality |
| **elfldr** | Loading ELF payloads |
| **PS5SX2 Installer / Helper** | Additional PS2 launch functionality |
| **Compatible emulators** | Running games on supported emulated platforms |

### 🔌 Important Ports

| Port | Service |
|---|---|
| **10199** | Launcher / Forwarder |
| **2121** | FTP server |
| **9021** | ELF loader |
| **10101** | ShadowMountPlus storage API |

**IMPORTANT: BGH does NOT automatically activate the required external services.**

Start the required services before using the corresponding game-launch functions.

---

## 🚀 Quick Start

1. **Activate your PS5 jailbreak and etaHEN.**
2. Start **ShadowMountPlus** if required for your setup.
3. Start **ftpsrv** on the configured FTP port.
4. Load the compatible **Launcher / Forwarder ELF**.
5. Start any additional emulator-specific helpers.
6. **Open BGH from the PS5 home screen.**
7. Configure your game folders and emulator paths.

**For detailed installation instructions, see the latest GitHub release.**

---

## 🕹️ Emulator Integration

### 🔵 PlayStation 2 – PS5SX2

BGH supports direct launching of PS2 games through the compatible PS5SX2 setup.

**Requirements:**

- Compatible PS5SX2 installation
- PS5SX2 Installer and Helper for the supported workflow
- Working FTP service
- Configured PS2 game directory

**Successfully tested with the PS5SX2 versions used during BGH development.**

### 🔵 PlayStation 3 – RPCS3-5

**BGH supports PlayStation 3 integration using RPCS3-5.**

**Confirmed in the developer's setup:**

- PS3 games are displayed in the BGH library.
- The PS3 emulator is integrated into the launcher workflow.
- **PS3 games can be launched directly through BGH.**
- Actual PS3 gameplay has been confirmed by the developer.

**Important:**

RPCS3-5 must be installed and configured separately.

Game compatibility depends on the emulator, installed version and individual title.

**Working PS3 emulation is confirmed for the tested configuration, not for every PS3 game.**

### 🔴 Nintendo Switch – ProsperoEden / ForgeNX

BGH includes emulator profiles for:

| Emulator | Application ID |
|---|---|
| **ProsperoEden** | `PPSA99008` |
| **ForgeNX** | `PPSA99186` |

**Both emulator configurations have been tested in supported setups.**

You only need one compatible Nintendo Switch emulator installed.

Configure the correct game folders and required launcher services.

### 🟢 Xbox 360 – PS5X360

BGH provides Xbox 360 game library and launch integration through PS5X360.

**Requirements:**

- Compatible PS5X360 installation
- Configured Xbox 360 game directory
- Required Launcher / Forwarder service

**Game launching has been confirmed in the tested PS5X360 setup.**

### Other Platforms

BGH also provides prepared library profiles for:

- PlayStation 1
- PlayStation Portable
- Original Xbox
- Sega Dreamcast
- Nintendo GameCube
- Nintendo Wii

**These profiles are prepared for library organization and future emulator integration. Direct game launching is not currently implemented for these platforms.**

---

## 💙 Support Development

BGH is a **free community homebrew project**.

If you enjoy using BGH and would like to support future development, you can leave a small voluntary donation.

**All support is optional. BGH remains free for everyone.**

### ☕ [Support bawertL on Ko-fi](https://ko-fi.com/bawertl)

Thank you for supporting my work!

---

## ❤️ Credits & Special Thanks

BGH benefits from the work of the PlayStation homebrew community.

Special thanks to:

- **John Törnblom / ps5-payload-dev** – ELF Loader, FTP Server and payload development
- **Swordpdf / PS5SX2** – PlayStation 2 emulator development
- **drakmor / ShadowMountPlus** – PS5 mounting tools
- **etaHEN developers and contributors**
- **RPCS3-5 developers and contributors**
- **ProsperoEden and ForgeNX developers**
- **PS5X360 developers**
- **All other PS5 homebrew contributors**

**BGH is an independent community project.**

External tools, payloads and emulators remain the work of their respective authors.

---

## ⚠️ Compatibility & Limitations

**Developed and tested on PS5 Firmware 13.60 with etaHEN.**

Other firmware versions, jailbreak environments and emulator configurations have not been fully verified.

**Important:**

- BGH is not an emulator.
- External emulators must be installed separately.
- Six additional platform profiles currently provide prepared library functionality only.
- Game launching depends on compatible software and external services.
- Game compatibility varies by emulator and title.
- Commercial games, console firmware and emulator BIOS files are not included.

---

## 📁 Project Information

**Project:** BGH – BawertL's Game Hub

**Version:** V1.1.32

**Developer:** bawertL

**Platform:** PlayStation 5 Homebrew

**Application ID:** `PPSA99079`

**Platform Profiles:** 12

**Confirmed Launch Platforms:** 6

---

<p align="center">
  <strong>🎮 One Game Hub. Multiple Consoles. One Library.</strong>
</p>

<p align="center">
  <strong>BGH – BawertL's Game Hub</strong>
</p>

<p align="center">
  Made by bawertL 💙
</p>
