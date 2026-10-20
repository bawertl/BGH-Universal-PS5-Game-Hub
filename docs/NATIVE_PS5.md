# PS5 Native Build / Integrationsstatus – V0.4 – 13.10.2026

## Technische Quellen, geprüft aus öffentlichen Repositories

- [PS5 Homebrew UI Kit](https://github.com/blackbearreloaded/ps5-homebrew-ui): `docs/ADOPTING.md`, `docs/BUILDING_A_DESIGN.md`, `src/concepts/concepts.hpp`, `src/app/shell.cpp`, `src/gfx/gl_batch.hpp`, `src/gfx/draw_list.hpp` und `src/platform/ps5/system.hpp`. Geringe Kopplung als einzelne `app::Concept`-Implementierung, native GL-Bilder über `DrawList::image`.
- [PS5 Native App Boilerplate](https://github.com/blackbearreloaded/ps5-native-app-boilerplate): Native C++20-Titel mit `eboot.bin`, `sce_sys`, `libc.prx`, vorbereiteter Titelordner für ShadowMountPlus; SDK und zlib werden beim Build heruntergeladen. **13.60 damit nicht automatisch bestätigt**.
- [PS5 Forwarder Format](https://github.com/heydemoura/ps5-forwarder-format/blob/main/SPEC.md): Port `127.0.0.1:10199`, 0x1020-Byte-Request, 0x18-Byte-Reply, `queued` Rückmeldungen; Startanfrage nur an einen bereits laufenden Launcher.
- [ProsperoEden Forwarder](https://github.com/blackbearreloaded/ProsperoEden/blob/main/docs/FORWARDER.md): `--rom <abs. Pfad>`, optional `--exit-after-game`, Ziel `PPSA99008`.
- [Forwarder Manager](https://github.com/heydemoura/ps5-forwarder-manager): PS5X360 nimmt einen einzelnen absoluten Pfad, Ziel `PPSA50011`; bestehende Forwarder werden über andere Payload-/Berechtigungsmechanismen erstellt.
- [PS5SX2](https://github.com/Swordpdf/PS5SX2/pulls): PR #25 „Adding launch argument to support forwarders“ (7. Oktober 2026). Nicht als bestätigte Release-Funktion behandeln.

## Build-Stufen und Belegstatus

| Stufe | Stand |
| --- | --- |
| Dateiscanner, Metadaten, Cache, PNG Decoder und Forwarder-Client (Host) | Mit Clang++17/GCC kompiliert, automatisierte Tests bestanden |
| C++20-GUI-Quellcode | Host-Syntaxkompilierung über API-ähnliche Test-Shims bestanden, **kein echter SDK-Build** |
| Vollständiger SDK/OpenGL-/PS5-Titel-Build | Hier **nicht möglich**: externes SDK konnte nicht in Container bezogen werden; in ZIP fehlt `eboot.bin` |
| Installation / Firmware 13.60 | **Ungetestet** |
| Konsolen-Dateizugriff `/data`, USB | **Nur am Zielgerät prüfbar** |
| Switch/Xbox 360 Direktstart | Protokoll / Hostsimulator geprüft, PS5 Hardwaretest ausstehend |
| PS2/PS4/PS5 Direktstart | Derzeit nicht nachgewiesen, UI verweigert Anfrage |

## Installation ist ein gesonderter, nicht freigegebener Schritt

Das Build-Skript wird nur auf Linux/WSL ausgeführt und sendet keine Daten an deine Konsole. Die Entwicklungs-ID `PPSA99079` ist nicht garantiert frei; vor Übertragung mit `/data/homebrew`, bestehenden Titeln und installierten Apps abgleichen. Die UI-Kit-Titelordner werden normalerweise unter `/data/homebrew/<TITLE_ID>` platziert und durch ShadowMountPlus registriert. Eine normale native App ist **keine** vom ELF-Loader direkt grafisch startbare Payload.

Die native App liest Spieleverzeichnisse ausschließlich. Schreiben geschieht nur im **eigenen** PS5-Titel-Sandbox-Verzeichnis `/download0/hui/ulg_v04/` für Cache/Präferenzen. Cover werden aus konfigurierten Quellen gelesen. Keine Installation oder Injection des Forwarder Launchers, keine weiteren Autoload-/Firmware-/etaHEN-/kstuff-Änderungen. Ohne vorhandenen Startdienst `10199` findet kein Direktstart statt. Ohne `/data`-Rechte findet die native App möglicherweise keine Spiele.

Risiken vor einer späteren Hardwareprüfung: App-Absturz auf Firmware 13.60, fehlende Sandbox-Dateirechte, Konflikte bei der Titel-ID, nicht kompatible SDK-Änderungen, fehlender Launch-Dienst. Vor Hardwaretest deshalb erst fertiges SDK-Build und Paketstruktur prüfen; **kein automatischer Upload**.

## Statusnachweise

1. `make -C native_ps5` – Kompilierung der hostseitigen nativen Kernprogramme; für die Entwicklungsrechnerarchitektur, **nicht** für PS5.
2. `python3 -m pytest -q tests` – 22 lokale Tests; kein echter PS5-Test.
3. `bash tooling/test_native_gui_syntax.sh` – Compile-only gegen Test-Shims, also **kein Nachweis**, dass das echte UI-Kit damit linkt.
4. `bash tooling/build_native_ps5.sh` – vorbereitet, hier **nicht erfolgreich ausgeführt**. Benötigt volle Upstream-Quelle, externe SDK-Bibliotheken und native Toolchain.

Keine realen Spiel-Dateien, Emulator-Installationen oder BIOS-Abbilder werden verteilt.
