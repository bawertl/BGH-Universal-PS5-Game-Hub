# V0.6 – Erster Konsolentest (noch nicht ausgeführt)

1. Nur ein erfolgreich in GitHub Actions gebautes und geprüftes PS5-Folder-ZIP ist installierbar. Host-ELF nicht verwenden.
2. Vor dem FTP-Update `/data/homebrew/PPSA99079` vollständig sichern; keine anderen Title-IDs ändern.
3. Nach dem Start prüfen, ob weiterhin GUI und Sound wie V0.5 erscheinen.
4. OPTIONS öffnet die Ordnerverwaltung. Fotografiere insbesondere die Fehlercodes (z. B. `2 / lstat`, `13 / opendir`).
5. Beobachten, ob `FTP lesbar` erscheint. Das nutzt nur das bestehende ftpsrv (v0.21.1, Port 2121), falls es auf der Konsole bereits läuft und aus dem Titel erreichbar ist.
6. Teste X (Pfad deaktivieren/aktivieren), Quadrat (Scan), Dreieck (Browser) und Touchpad (Pfad bearbeiten). Testpfade nur speichern, wenn du die Änderung möchtest.
7. Wenn keine Spiele erscheinen, die fehlerhaften Operationen/errno dokumentieren. Nicht an Jailbreak/Autoload herumprobieren.

WICHTIG: Die V0.6 nimmt **keine** Rechteanhebung vor. Das offizielle Boilerplate dokumentiert als optional eine Lapy-Berechtigungsanfrage, aber für die konkrete Firmware 13.60 und vorhandene Payload-Kombination gibt es keinen nachgewiesenen Sicherheitstest dieses Wegs. Er bleibt ausgeschaltet und wird nicht ungefragt eingebaut.
