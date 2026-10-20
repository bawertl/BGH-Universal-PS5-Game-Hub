# PS5 Universal Game Launcher – Diagnose-/Ordner-Version (14.10.2026)

## Nachweisbarer Status

- Programmquelle: V0.5 weiterentwickelt, funktionierende Homebrew-UI-Grafik, Sound und Controller-Integration beibehalten.
- Lokale Unit-/Integrationsprüfungen: **37/37 bestanden** (Linux), einschließlich Loopback-FTP-Test mit simuliertem Dienst.
- Native Scanner-, Zustandsspeicher-, FTP- und Ordnerverwaltungsmodule: auf Linux als C++20-Binärdateien kompiliert.
- Native GUI: erfolgreich mit Host-Shim gegen UI-Kit-Schnittstellen syntaktisch geprüft; **kein erfolgreicher PS5-Link als Teil dieses Quellcodepakets**.
- Echte PS5-Kompilierung: benötigt weiterhin einen GitHub-Actions-Runner mit PS5-UI-Kit/SDK. Das ZIP ist **KEIN** fertig installierbares PS5-Paket.
- Firmware-13.60-Test: V0.5 startet nach Nutzerfotobeleg; die *neuen* Funktionen sind auf Hardware noch nicht getestet.

## Änderungen

1. Einzelner POSIX-Status pro Suchpfad mit Operation (`lstat`, `opendir`, `readdir`, `type`) und numerischem `errno`.
2. Separate Status: OK, nicht vorhanden, USB-Mountpunkt fehlt, Zugriff verweigert, kein Ordner, sonstiger Fehler, deaktiviert, FTP lesbar.
3. Hauptpfadfehler in die Gesamtzahl einbezogen; nur aktive Pfade zählen als fehlgeschlagen. Zusätzliche Unterverzeichnisfehler sind eigene Ereignisse.
4. Controllerbedienbare Ordnerliste: X aktiviert/deaktiviert, Quadrat erneuter Scan, Dreieck öffnet Browser, Touchpad öffnet Pfadeditor; L1/R1 wählt Plattform zum Hinzufügen.
5. Ordnerbrowser zeigt ausschließlich Verzeichnisse. Bei eingeschränktem Zugriff kann eine Controller-Bildschirmtastatur einen absoluten Pfad eingeben. Einstellungen werden nur im privaten App-Speicher gespeichert.
6. Optionaler **lesender** FTP-Fallback zum bestehenden `ftpsrv` auf `127.0.0.1:2121` (offiziell dokumentierter Standardport). Ausschließlich USER/TYPE/CWD/PASV/MLSD/optional RETR; kein Hochladen, Löschen, kein Payload-Start, keine Rechteanhebung.
7. Darstellungs-/Fehlerbehebung: Fehlende POSIX-Lesbarkeit führt nun nicht mehr automatisch zu „0 Fehler“. Der lokale FTP-Status wird nach einem frischen Scan separat angezeigt.

## Nicht nachgewiesen

- Ob `127.0.0.1:2121` aus der Sandbox der PS5-App auf FW 13.60 tatsächlich erreichbar ist.
- Ob ein vorhandener ftpsrv-Dienst alle benötigten Spielverzeichnisse von der Konsole aus lesen darf.
- Ob die Spieldateien sämtlicher Plattformen über FTP korrekt als Spiel erkannt werden; heuristische Erkennung bleibt.
- Ob lokal nicht zugängliche Cover aus FTP-Daten als Grafik geladen werden (der aktuelle PNG-Lader braucht einen lesbaren lokalen Pfad).
- Neue native GUI noch nicht auf PS5 kompiliert oder ausgeführt.

## Sicherheit

Das Projekt ändert weder Spiele noch Speicherstände, Emulatoren, Jailbreak, Autoload oder PS5-Firmware. FTP ist ausschließlich über Loopback angesprochen und read-only. Es wird keine neue Serverinstanz gestartet; falls der vorhandene Dienst nicht erreichbar ist, bleibt die POSIX-Diagnose erhalten. Einzelne privat gespeicherte Launcher-Pfade/Favoriten sind die einzigen beabsichtigten Schreibzugriffe.
