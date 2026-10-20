# PS5 Universal Game Launcher V0.5 – tatsächlich geprüfter Build-Blocker

**Datum:** 14.10.2026. **Zielhardware:** PS5 Firmware 13.60.

## 1. Was konkret versucht wurde

Aus dem vorhandenen V0.4-Quellcode wurde die Build-Prozedur `bash tooling/build_native_ps5.sh` ausgeführt. Das Skript soll das offizielle OpenGL/UI-Kit von GitHub holen, dann dessen Build-Werkzeuge auf den eigenen native-Launcher-Quellcode anwenden.

Tatsächlicher originaler Fehler im Entwicklungscontainer:

```text
Cloning into '.../build/ps5/ps5-homebrew-ui'...
fatal: unable to access 'https://github.com/blackbearreloaded/ps5-homebrew-ui.git/': Could not resolve host: github.com
```

**Exit-Code:** 128. Derselbe Host kann `github.com` über `getent hosts` nicht auflösen. Es fehlen außerdem der vollzählige PS5-UI-Kit-Checkout, das PS5 Payload SDK, PS5-OpenGL-Importbibliotheken sowie die tatsächlich zugehörigen Build-Konverter in diesem Container. Die verfügbaren Clang-/GCC-Compiler sind allein kein Ersatz.

## 2. Gegenprüfungen

- PS5-UI-Kit (`blackbearreloaded/ps5-homebrew-ui`) und vollständige Build-Anforderungen anhand der offiziellen README und Quellcode-Pfade verifiziert. Gepinnter GitHub-Commit: `bbe19054ee2e7ec069254775273aeb8846c4bd3e` vom 13.10.2026.
- Native Hostmodule mit C++20 / `clang++ -O2 -Wall -Wextra -Wpedantic -Werror` gebaut.
- Automatisierte Host-Regressionstests mit pytest: **25 bestanden**.
- GUI nur mit lokalen Host-Shim-Headern syntaktisch überprüft, **nicht** mit der PS5-OpenGL-Toolchain gelinkt.
- PS5 `eboot.bin`, `sce_module/libc.prx` und finaler Titelordner **nicht** vorhanden.
- Auf echter PS5 13.60 **nicht** getestet.

## 3. Geprüfte Alternative – netzfähige CI

Das Projekt enthält `.github/workflows/ps5-build.yml`. Auf einem GitHub-Actions-Linux-Runner soll dieses:

1. LLVM/Clang 18 und übrige Build-Werkzeuge installieren.
2. Den gepinnten PS5-UI-Kit-Quellcode klonen.
3. V0.5 als einzelnes UI-Concept einbinden.
4. Upstream-SDK, OpenGL-Bibliotheken und Runtime nach offiziellen Build-Skripten laden/erstellen.
5. Nur bei erfolgreichem PS5-Build den Ordner mit `eboot.bin`, `sce_module/libc.prx`, `sce_sys/param.json`, Assets prüfen und zu **einem** PS5-App-ZIP verpacken.
6. Ergebnis plus SHA-256-Prüfsumme als CI-Artefakt ablegen.

Dieser Weg wurde mangels für dieses Projekt verfügbarem GitHub-Repository/CI-Runner **noch nicht tatsächlich ausgeführt**. Er ist kein Nachweis eines erfolgreichen Console-Builds.

## 4. Weitere reale Konsole-Einschränkungen

Auch bei erfolgreichem Build ist Firmware-13.60-Kompatibilität nicht vorab belegt. Eine Sandbox-Anwendung besitzt eventuell keinen `/data`-/USB-Lesezugriff; ohne diesen kann die Bibliothek nur eingeschränkt funktionieren. Das Programm löst bewusst **keine** automatischen Jailbreak-/Elevations-Payloads aus. Startanfragen für ProsperoEden/PS5X360 benötigen den **bereits vorhandenen** Forwarder-Dienst auf `127.0.0.1:10199`. Das Vorhandensein von etaHEN und ShadowMountPlus alleine beweist diesen Dienst nicht. PS5SX2-Direktstart und native PS5/PS4-Direktstarts sind nicht freigeschaltet.

## 5. Sichere Installation – ausschließlich nach erfolgreichem echten Build

Erst nach einer tatsächlich erstellten und überprüften PS5-App ZIP: Vorher auf der Konsole prüfen, ob die Entwicklungs-Title-ID `PPSA99079` wirklich frei ist. **Nicht überschreiben, wenn dort schon etwas installiert ist.** Die ZIP muss einen kompletten Titelordner `PPSA99079/` einschließlich `eboot.bin`, `sce_module/libc.prx`, `sce_sys/param.json` und `assets/` enthalten. Diesen *Ordner*, nicht einzelne Dateien oder die ZIP selbst, per FTP nach `/data/homebrew/` übertragen. ShadowMountPlus kann den Titel registrieren; Registrierung/Launch erst nach ausdrücklicher Freigabe auf dem Gerät. Keine Änderungen an etaHEN, kstuff oder Autoload.

**Gesicherte Schlussfolgerung:** Die gegenwärtige Entwicklungsumgebung kann die notwendige externe PS5-Toolchain nicht laden und daher keinen belastbaren nativen PS5-Build erzeugen. Eine funktionsfähige Linux-ELF wäre kein Ersatz.
