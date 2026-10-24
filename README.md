# BGH V1.1.14 – ShadowMountPlus-Speicheranzeige (PS5-Testversion)

**Ausgangsbranch:** `bgh-v1.1.13-ui-persist` → **neuer Branch:** `bgh-v1.1.14-shadowmount-storage`. **App-ID/PS5-Ordner:** `PPSA99079` (unverändert).

Der Launcher liest nach 8 Sekunden erstmals und anschließend frühestens alle 60 Sekunden ausschließlich `POST http://127.0.0.1:10101/api/v1/storage` mit Body `{}`. Benötigt wird ein **bereits laufendes ShadowMountPlus mit kompatibler HTTP-Schnittstelle**; der Launcher startet oder konfiguriert weder ShadowMountPlus noch weitere Payloads.

Aus der Antwort werden ausschließlich reale, validierte Bytewerte für `/user` sowie `/mnt/usb*` und `/mnt/ext*` in den zwei Speicher-Widgets angezeigt. Mehrere externe Dateisysteme werden anhand des `source`-Feldes dedupliziert und zusammengefasst. Die Balken zeigen `used_bytes / total_bytes`; die freie Kapazität verwendet `available_bytes`. Textgrößen entsprechen dezimalen GB/TB (10^9 / 10^12 Byte). Sind keine gültigen Daten vorhanden, bleiben nur Statusinformationen sichtbar – keine erfundenen Werte und kein voll ausgefüllter Dummy-Balken.

**Vorsicht:** Das ist die erste experimentelle HTTP-Anbindung. Sie arbeitet mit begrenzten lokalen Socket-Timeouts und ohne zusätzlichen C++-Thread, kann daher bei einer langsam antwortenden API einen kurzen Frame-Stillstand auslösen. Die reale PS5-API-Verfügbarkeit, vollständige Paketkompilierung und Laufzeitstabilität sind erst durch GitHub-Build und den PS5-Test bestätigt. Kein `statvfs()` im neuen API-Pfad. Diagnose unter **Einstellungen → System & Hilfe → ShadowMount HTTP**.

**Bestehende Startumgebung:** ShadowMountPlus läuft bereits; die Launcher-ELF wird für bestehende Spielstarts benötigt; PS2-Helper-ELF wie bisher für PS2-Direktstarts. V1.1.14 verändert diese ELFs oder Startwege nicht.

---

# BGH V1.1.4 – USB-Scan-Reparatur (aus stabiler V1.1.3) 

**Ausgangsbranch:** `bgh-v1.1.3` → **neuer Branch:** `bgh-v1.1.4-usb-rebuild`.
**Änderungen:** Alle 12 Plattform-/Konsolen-Icons (PS5, PS4, PS2, Switch, Xbox 360, PS1, PSP, PS3, Original Xbox, Dreamcast, GameCube und Wii) als einheitlich freigestellte 96×96-PNGs. Der Emulator-/Plattform-Manager hat eine echte, kontextabhängige PlayStation-Tastenleiste einschließlich L2/R2, Quadrat, Dreieck und Touchpad. Ordnerbrowser und Pfadeditor nutzen dieselben nativen Glyphen. Bestehende Starts, Sortierung, Pfade, Musik, Cover-Cache und Verlauf bleiben unverändert.
**GitHub-Artefakt:** `BGH-V1-1-4-USB-R1-PPSA99079-TEST`.
**Grenze:** PS5-Gerätetest folgt nach GitHub-PS5-Build; Host-Tests sind kein Hardware-Nachweis.

---

# BGH V1.1.1 – Universal Platform & Emulator Manager

**Ausgangsbranch:** `bgh-v1.1.0` → **neuer Branch:** `bgh-v1.1.1`.
**Änderungen:** Globale Sortierung aller Konsolen und Emulatoren, ausklappbare Anwendungsdetails, mehrere Spielordner pro Plattform, erweiterte Registry mit V1.1.0-Migration und einheitliche monochrome Icons. Bestehende Scans, Startwege, Musik und Verlauf bleiben separat.
**Build-Artefakt:** `BGH-V1-1-1-PPSA99079-TEST`.

---

# BGH V1.0.10 – Cover Fit & Performance

**Ausgangsbranch:** `bgh-v1.0.9` → **neuer Branch:** `bgh-v1.0.10`.

**Änderungen:** PS2-/Xbox-/Homebrew-Cover werden ohne seitlichen Zuschnitt und ohne Verzerrung proportional eingepasst. PS4-/PS5-/Switch-Banner bleiben unverändert. Cover-Cache von 48 auf 256 Texturen erweitert (Spielbilder maximal 512 px Kantenlänge); Hauptcover und direkte Nachbarn werden vor der 35-ms-Listen-Debounce geladen, jedoch maximal ein neuer synchroner Cover-Ladevorgang pro Frame. Bereits geladene Bilder bleiben sichtbar.

**Prüfgrenze:** Eine reale FPS- oder PS5-GPU-Speichermessung ist erst am Gerät möglich.

---

# BGH V1.0.9 – Final Polish und Emulator-Menü (Bugfix)

**Ausgangsbranch:** `bgh-v1.0.7` → **neuer Branch:** `bgh-v1.0.10`; Title-ID `PPSA99079`.

**In V1.0.6 umgesetzt:** Im Tab **Darstellung & Verhalten** schaltet **X** zwischen dem bisherigen Direktstart (Standard) und einem reinen Emulator-Menüstart um. Das gilt ausschließlich für PS2, Switch und Xbox 360. PS4 und PS5 starten wie vorher. Ein Menüstart erstellt weder PS2-`disc-launch.txt` noch einen Emulator-ROM-Startauftrag und wird nicht als gespieltes Spiel im Verlauf erfasst. Die Beschriftung von X passt sich im Spielbereich an.

**Rückwärtskompatibel:** Das bestehende `ULGP`-Format (`preferences.bin`) samt Favoriten und Verlauf bleibt unverändert. Die Einstellung nutzt eine getrennte `ULGE`-Datei (`emulator_boot.bin`) mit Version, monotonem Revisionszähler und Prüfsumme in den beiden privaten BGH-Speicherorten. Bei unvollständigen Schreibvorgängen gewinnt die neuere gültige Kopie. Wenn keine gültige Datei vorhanden ist, gilt sicher der bisherige Direktstart.

**Neu in V1.0.9:** Deutsch/Englisch wird im Tab „Darstellung & Verhalten“ über Hoch/Runter + X gewählt. Die Übersetzungslogik umfasst die zentralen Menüs, Such-/Ordner- und Spieleansichten, Controller-Hilfe und fest definierte Bedienhinweise. Stabile Spieltitel, Pfade und Diagnose-Rohdaten bleiben unverändert. Die Sprachwahl wird redundant und prüfsummengeschützt als ULGL gespeichert, ohne Favoriten-/Verlaufsdatei ULGP oder Emulator-Modus-Datei ULGE zu migrieren.

**Build:** GitHub Actions auf `bgh-v1.0.10`, Artefakt `BGH-V1-0-10-PPSA99079-TEST`.

---

## Historischer Stand: BGH V1.0.3 – UI & Controller Upgrade (Block 3)

**GitHub:** Ausgangsbranch `bgh-v1.0.2` → neuer Branch `bgh-v1.0.3`. **Title-ID:** `PPSA99079`.

**Änderungen:** fünf Cover weiterhin symmetrisch, jetzt geringfügig weiter auseinander; Titel 43 px mit verstärkter Kontur; konservative Bereinigung von Release-Tags wie `(nsw2u)`, `[T-En]` und `[v1.2.3]` auch bei Metadaten und Cache-Titeln. Echte Namensbestandteile wie `(Remastered)` bleiben erhalten. Unten rechts erscheint `BGH V1.0.3`. Aktive Controller erhalten einen dezenten warmen Glow und eine Status-LED; Akku-Kapsel und das Datenmodell sind vorbereitet.

**Wichtig zur Akkumessung:** Die öffentlich dokumentierte 120-Byte-PS5-Pad-Struktur enthält keine gesichert zugeordnete Akku- und Ladeinformation. Die früher vorgeschlagenen Bytes `0x54` / `0x55` sind Teil des Zeitstempels. Deshalb zeigt BGH beim verbundenen Controller vorläufig `--` statt einer erfundenen Prozentzahl; ein Ladezeichen erscheint nur bei bestätigter Information. Kein unbestätigtes Raw-Byte-Decoding aktiviert.

**Unverändert:** PS5/PS4/PS2/Switch/Xbox-Startwege, Controller-Eingabeverhalten, Scanpfade, ULGP-Speicherformat, Verlauf und USB-/Cover-Zuordnungen.

**GitHub Actions:** `bgh-v1.0.3` → Artefakt `BGH-V1-0-3-PPSA99079-TEST`, PS5-App-ZIP `BGH_BawertLs_Game_Hub_V1_0_3_PPSA99079_PS5_TEST.zip`.

---


# BGH V0.12.27 – Centered Fan, Direct Launch & Recent

**GitHub:** `bgh-v0.12.23` → `bgh-v0.12.24`. **Title-ID:** `PPSA99079`.

V0.12.27 zentriert den Cover-Fächer anhand der tatsächlichen horizontalen UI-Grenzen, entfernt die zwei oberen Kopf-Kacheln und zeichnet einen feinen champagnerfarbenen dynamischen Hero-Rahmen. Die bereitgestellten neuen PS5-, PS4- und Switch-PNG-Banner ersetzen die alten Assets.

In V0.12.27 startete **X** das ausgewählte Spiel, **L1** öffnete damals Details und **R1** wechselte Plattform. Seit V0.12.27 wechseln L1/R1 wieder die Plattform und L3 öffnet Details. Die zuletzt angeforderten Spiele sind via Touchpad fokussierbar. X auf einer Verlaufskarte startet die zugeordnete reale Bibliothekseintragung. Die Verlaufsliste verwendet stabile Plattform+Pfad-Schlüssel und vermeidet Dopplungen. Ab V1.0.0 speichert sie Startaufträge vor der Übergabe an den Forwarder und rollt eindeutige Ablehnungen zurück. Eine erfolgreiche Spielelaufzeit ist dadurch **nicht** nachgewiesen.

**Build:** GitHub Actions erstellt `BGH_BawertLs_Game_Hub_V1_0_3_PPSA99079_PS5_TEST.zip`; der native PS5-Hardwaretest steht noch aus.

---

# BGH V0.12.23 – Typography, Recent & Fan Polish

**GitHub:** `bgh-v0.12.22` → `bgh-v0.12.23`. **Title-ID:** `PPSA99079`.

**Hauptoberfläche:** Serifenschrift aus einer vorgerenderten transparenten PNG-Glyphen-Atlasdatei (keine Schriftdatei im Archiv), BGH-Branding kompakter, Plattform- und dynamische Listentexte ebenfalls im Serifstil. Cover-Fächer um 122 logische Pixel weiter nach links verschoben, äußere Flügel enger gestaffelt und mit sanfteren Schatten. Controlleranzeige oben vertikal zentriert. „Zuletzt gespielt“ und die drei Karten tiefer platziert; die PlayStation-Kreuz-Schaltfläche wurde vertikal korrigiert.

**Verlauf:** Der vorhandene Preferences-Mechanismus bleibt bestehen. Zusätzlich sichert der Launcher jüngste Spiele unter `recent_v123.bin` mit Plattform/Pfad-Wiedererkennung im selben privaten BGH-Verzeichnis. Bei Schreibfehlern beider Speicherversuche zeigt die Oberfläche einen Hinweis. Der tatsächliche PS5-Speicherzustand kann erst am Gerät überprüft werden.

**Build-ZIP:** `BGH_BawertLs_Game_Hub_V0_12_23_PPSA99079_PS5_TEST.zip`. Der PS5-/GitHub-Build ist erst nach Ausführung bestätigt.

---

# BGH V0.12.22 – Layout Polish / Glass Refinement

**GitHub:** `bgh-v0.12.21` → `bgh-v0.12.22`. **Title-ID:** `PPSA99079`.

Ausschließlich visuelles Feintuning der V0.12.21: Der dynamische Cover-Fächer sitzt weiter links; das gewählte Cover wird stärker hervorgehoben; die schwarze Info-Kachel unter dem Fächer entfällt. Dort stehen nur noch der Spieltitel, ein kompaktes Plattform-Badge und ein transparenter „Spiel starten“-Button mit dem bereits integrierten PlayStation-Kreuz-Glyph. Die drei „Zuletzt gespielt“-Karten sowie die Sidebar-/Bibliotheks-/Kopfbereichsgläser sind transparenter. „Verfügbar“ und „Installationsordner“ werden in dieser Hauptansicht nicht mehr angezeigt. Im Detailfenster bleibt die zusätzliche Startbestätigung erhalten; nur die Bezeichnungen wurden bereinigt.

**Unverändert:** Launch-, Forwarder-, FTP-, Scan-, Coverzuordnungs-, Controller- und Speicherroutinen. Keine Änderungen an Jailbreak oder Title-ID.

**Build-ZIP:** `BGH_BawertLs_Game_Hub_V0_12_22_PPSA99079_PS5_TEST.zip`. Der PS5- und GitHub-Build ist erst nach der entsprechenden Ausführung bestätigt.

---

# BGH V0.12.18 – PS2-Direktstart über localhost-FTP (TEST)

**Ausgangslage:** Die echten PS5-Tests mit V0.12.17 ergaben: PS5SX2 öffnet das Menü, aber das per POSIX `open()` von BGH erzeugte `disc-launch.txt` ist für WinSCP nicht zu sehen und löst keinen Direktstart aus. Der **manuelle FTP-Upload** derselben Request-Datei startete Virtua Fighter 4 nachweisbar direkt (`boot.4.log`). Unterschiedliche Dateisystemsichten sind die Arbeitshypothese, nicht zu 100 % belegt.

**Neu in V0.12.18:** Bei X auf einem PS2-Spiel prüft BGH per bestehenden localhost-FTP-Lesefunktionen, ob im realen `/data/PCSX2/logs/` bereits eine `disc-launch.txt` vorhanden ist. Falls nicht, überträgt ein eng begrenzter separater FTP-Schreiber mit `STOR` den exakten, validierten Dateipfad und einem abschließenden Zeilenumbruch. Nur wenn der FTP-Server mit 226/250 bestätigt **und** `RETR` denselben Inhalt zurückliefert, wird der unveränderte externe Forwarder (Port 10199) zum Öffnen von `PPSA99203` aufgerufen. `Quadrat` öffnet weiter nur das PS5SX2-Menü.

**Schutz und Grenzen:** Es gibt keinen stillen POSIX-Fallback und kein automatisches Löschen fremder Dateien. Ein vorhandener Auftrag stoppt den Start; eine nicht verfügbare/lesbare FTP-Verbindung ebenfalls. Nur bei abgelehntem Forwarder-Start wird ein in diesem Aufruf geschriebener, inhaltlich unveränderter Auftrag über FTP gelöscht. `MLSD`, `STOR` und `RETR` müssen vom vorhandenen lokalen FTP-Dienst unterstützt werden. Weil Standard-FTP kein bedingtes `STOR` bietet, bleibt bei parallelen externen Schreibern ein kleiner Prüf-/Schreibrace; es wird ausschließlich der feste Request-Dateiname verwendet. Im Zweifelsfall nach einem Fehlversuch FTP-Ordner kontrollieren. Kein Payload-Autostart und kein eigenständiger PS5-Build vorgetäuscht.

**Unberührt:** PS5-, PS4-, Switch- und Xbox-360-Startzweige, FTP-Reader für Scanner/Covers, Emulatoren, USB-Spiele, UI und Design. Nach dem GitHub-Build ist ein **echter Konsolentest** notwendig.

---


# BGH V0.12.16 – PS5SX2-Menustart via Forwarder (Test)

- Basis: auf PS5 13.60 praktisch getestete V0.12.15.
- PS5, PS4, Switch und Xbox 360: existierende Startwege unverändert.
- PS2: in der Detailansicht X → **PPSA99203 ueber den bereits laufenden Forwarder auf Port 10199 oeffnen**.
- Absichtlich **keine** PS2-ROM-Argumente oder Dateipfad-Uebergabe: oeffnet nur PS5SX2 und dessen Spielauswahl.
- Ohne laufenden Forwarder: eine Fehlermeldung, kein Injektionsversuch, kein nativer Fallback.
- Diese Funktion ist noch **nicht auf der echten PS5 getestet**.

# BGH V0.12.15 – PS4-CUSA-Starttest (Basis V0.12.14)

- Separate PS4-CUSA-Startplanung mit strenger Title-ID-Validierung.
- Der bestehende Startdienst auf Port 10199 bleibt erforderlich; kein Payload-Autostart.
- Leere/kleingeschriebene CUSA-IDs werden im Katalog vor der Deduplizierung normalisiert.
- PS4-Detailfenster zeigt die zu startende Title-ID und dieselbe TCP-Diagnose.
- Der zuvor erfolgreich getestete PS5-Startzweig und `ps5_launch.cpp` bleiben unverändert.
- Keine Änderungen an Covern, Emulatorscans, FTP, Systemdatenbank oder Jailbreak.
- PS4-Start wurde spaeter mit V0.12.15 auf der echten PS5 bestaetigt.

---

# BGH V0.12.15 – Teststart registrierter PS4-CUSA-Spiele

- Neue separate PS4-Startplanung für 9-stellige `CUSAxxxxx`-Kennungen.
- Verwendet dieselbe bereits für PS5 getestete `send_launch()`-Bridge
  zu einem manuell gestarteten Forwarder auf Port 10199.
- Korrigiert fehlende/kleingeschriebene PS4-Title-IDs beim Bereinigen des
  Katalogs, bevor die PS4-Deduplizierung vorgenommen wird.
- Detailansicht zeigt bei PS4 die erkannte Title-ID und bietet den
  bisher bewährten Quadrat-Netzwerktest.
- PS5-Startzweig, `ps5_launch.cpp`, Emulatoren, Coverdecoder, FTP und
  Loopback-Diagnose werden nicht verändert.
- Kein automatischer Launcher-Payload, kein PS4-Fallback ohne Daemon,
  keine Jailbreak-/Spieledateien- oder Systemdatenbankänderungen.
- Bei Erstellung von V0.12.15 war PS4-Hardwareverhalten noch zu testen: `queued`, `started` und echte
  Spielausführung unterscheiden.

---

# BGH V0.12.15 – Teststart registrierter PS4-CUSA-Spiele

- Neue separate PS4-Startplanung für 9-stellige `CUSAxxxxx`-Kennungen.
- Verwendet dieselbe bereits für PS5 getestete `send_launch()`-Bridge
  zu einem manuell gestarteten Forwarder auf Port 10199.
- Korrigiert fehlende/kleingeschriebene PS4-Title-IDs beim Bereinigen des
  Katalogs, bevor die PS4-Deduplizierung vorgenommen wird.
- Detailansicht zeigt bei PS4 die erkannte Title-ID und bietet den
  bisher bewährten Quadrat-Netzwerktest.
- PS5-Startzweig, `ps5_launch.cpp`, Emulatoren, Coverdecoder, FTP und
  Loopback-Diagnose werden nicht verändert.
- Kein automatischer Launcher-Payload, kein PS4-Fallback ohne Daemon,
  keine Jailbreak-/Spieledateien- oder Systemdatenbankänderungen.
- Bei Erstellung von V0.12.15 war PS4-Hardwareverhalten noch zu testen: `queued`, `started` und echte
  Spielausführung unterscheiden.

---

# BGH V0.12.14 – Blockierender TCP-Verbindungstest (Basis V0.12.13)

- Damaliger Branch `bgh-v0.12.14`; BGH-Title-ID bleibt `PPSA99079`.
- Die **rein manuelle** Quadrat-Diagnose in der PS5-Detailansicht nutzt jetzt einen
  blockierenden `connect()` wie der vorhandene BGH-Startclient und FTP-Client.
- Entfernt `fcntl(F_GETFL/F_SETFL)` und `select()` **nur aus dem Porttest**,
  weil `fcntl()` auf der echten PS5 mit `EACCES (13)` abbrach, bevor `connect()` lief.
- Weiterhin ausschließlich `127.0.0.1:10199`, `:9021` und `:2121`, jeweils
  `connect()` plus `close()`, **keinerlei Payload-Daten oder Startbefehle**.
- Sendet den `errno` von `connect()` und Fehlerstufe `CONNECT` an die bestehende
  Diagnoseanzeige. Ein erreichbarer TCP-Port beweist keinen PS5-Spielstart.
- Best-effort `SO_SNDTIMEO/SO_RCVTIMEO`; ein festes 300-ms-Limit für einen
  blockierenden `connect()` ist plattformübergreifend nicht garantiert.
- Kein automatischer Payload-Start, keine Änderung an etaHEN/ShadowMountPlus,
  Spielebibliothek, Cover-Suche oder ursprünglicher PS5-Start-Bridge.

---

# BGH V0.12.13 – Socket-Fehlerdiagnose (Basis V0.12.12)

- Neuer Branch `bgh-v0.12.13`; BGH-Title-ID unverändert `PPSA99079`.
- Neue, ausschließlich manuell per **Quadrat** auf der PS5-Spieldetailseite ausgelöste Diagnose: TCP-Verbindungen auf Loopback-Ports `10199` (Forwarder Launcher), `9021` (elfldr) und `2121` (FTP, bestehender BGH-Fallback) prüfen; errno und Fehlerphase anzeigen.
- Kein Senden von Payload-/Protokolldaten; kein Starten, Installieren oder Verändern eines Dienstes.
- Die Anzeige `ERREICHBAR` bedeutet nur, dass ein TCP-Listener antwortet. Ob dieser tatsächlich der erwartete Dienst ist, lässt sich dadurch nicht beweisen.
- BGH V0.12.11-Spielstart und sämtliche Cover-/Scannerfunktionen bleiben unverändert.
- Reiner Host-/GUI-Syntaxtest ist kein erfolgreicher nativer PS5-Build oder PS5-Laufzeittest.

---

# BGH V0.12.10 – Xbox-360-USB-Cover-Verknüpfung (Basis V0.12.9)

- Branch: `bgh-v0.12.10`, Title-ID weiterhin `PPSA99079`.
- Sichtbare Kennung: `BGH V0.12.10-XBOX-COVER-R1 [Git-Commit]`.
- Ergänzter **lesender** Coverordner: `/mnt/usb1/xbox360/Cover/`.
- Exakte vom Nutzer bestätigte JPG-Dateinamen: `Gears of War 2.jpg`,
  `Lord of the Rings War in the North.jpg`, `Viva Party Animals.jpg`.
- Gears 2 und LOTR werden über bestätigte Xbox-Titel-IDs verknüpft; Viva Piñata
  durch eindeutigen Spielnamen/ISO-Pfad (ohne erfundene Titel-ID).
- Bei fehlendem JPG bleiben ursprüngliche PS5X360-/ISO-Coversuchen aktiv.
- Original-JPGs, Xbox-ISOs, Emulatoren und sämtliche anderen Decoder unverändert.
- **Nicht Teil dieses Auftrags:** PS5 `.ffpfsc`/`.exfat`-Erkennung und Direktstart.
- Ein positiver Hosttest garantiert keinen PS5-Laufzeit-Erfolg.

---

# BGH V0.12.9 – Bibliotheksbereinigung und PS5-Coverwiederherstellung (Basis V0.12.8)

Technische Basis: die auf echter PS5 getestete BGH V0.12.8; PS4-, PS2- und Switch-Cover waren dort funktionsfähig.
Neue sichtbare Kennung: **BGH V0.12.9-LIBRARY-CLEAN-R1 [Git-Commit]**.
Branch: **bgh-v0.12.9**, Title-ID bleibt **PPSA99079**.

## V0.12.9 – Neue Bibliotheksregeln

- **Nur Spiele in den Spielekategorien:** installierte native PPSA-Titel unter PS5,
  CUSA-Titel unter PS4 und ROMs unter ihren jeweiligen Emulator-Plattformen.
- **Emulatoren separat:** PS5SX2, ProsperoEden, PS5X360 bleiben im Reiter Emulatoren.
  Homebrew-Dienstprogramme (App Dumper, AriaNg, BlackBox, etaHEN, Orbit Store usw.)
  erscheinen nicht als Spiele.
- **Keine Mehrfacheinträge:** Gleiche bestätigte PPSA-/CUSA-Kennungen aus
  mehreren Pfaden werden zu einem bevorzugten Eintrag zusammengeführt.
  ROM-Kopien mit identischem Anzeigenamen und Dateiformat werden ebenfalls
  einmal angezeigt. Es wird **keine** Datei gelöscht, verschoben oder verändert.
- **Suchordner AN/AUS funktioniert:** Deaktivierte Ordner füllen die Bibliothek
  nicht mehr aus dem Offline-Cache. Angeschlossene und nicht angeschlossene
  *aktivierte* Quellen bleiben unterscheidbar. Neuer eigener privater Katalogcache.
- **PS5-Coverwiederherstellung:** ergänzende lesende Suche in
  `/user/appmeta/PPSAxxxxx/icon0.png` (POSIX oder vorhandener lokaler FTP-Dienst),
  auch für PS5-Dumps mit der PPSA-Kennung innerhalb des Ordnernamens.
  Wenn eine vorhandene PNG-Datei für BGH nicht lesbar ist, wird der private
  Covercache optional per FTP neu befüllt. Auf einer PS5 noch nicht bestätigt.
- **Einfachere Einstellungen:** L1/R1 filtert Ordner nach Plattform;
  deutliches AN/AUS, Anzeige der Anzahl aktiver Quellen sowie Hilfe zur Bedienung.
- Alle bestehenden PS4-, PS2- und Switch-Coverdecoder und der PS5-Spielstart
  bleiben unverändert. PS4-Cover und PS2-/Switch-Cover sind nach V0.12.8
  auf echter PS5 bestätigt; V0.12.9 muss dort erst getestet werden.

## Aus V0.12.8 übernommene PS4-PKG-Erkennung

- Verifiziert anhand des Benutzer-FTP-Screenshots: `/user/app/CUSAxxxxx/app.pkg` und getrennte PS4-Icons `/user/appmeta/CUSAxxxxx/icon0.png`.
- CUSA-Ordner mit tatsächlichem `app.pkg` gelten nun als PS4-Installation, auch ohne `sce_sys/param.sfo` oder `eboot.bin`. Einzelne heruntergeladene PKG-Dateien oder nur zufällig passende Verzeichnisnamen werden nicht als installiert angenommen.
- Native POSIX-Suche und read-only lokaler FTP-Fallback unterstützen diese Ordnerform. PS4-Icons werden lesend direkt oder per FTP in den privaten BGH-Cache übernommen; Spielverzeichnisse werden nie verändert.
- `/user/app` wird sowohl als paketierter Pfad als auch append-only zu alten BGH-Benutzer-Suchpfaden ergänzt, ohne vorhandene Pfade/Deaktivierungen zu verändern.
- Für die sieben über Fotos und öffentliche Titelzuordnungen identifizierten CUSA-IDs werden die Namen aus `assets/titles.conf` angezeigt. Andere Titel behalten einen ehrlichen CUSA-Fallback; keine angebliche automatische Online-Metadatenerkennung.
- Der PS5-/PS4-Direktstart und die bekannten Emulator-Coverdecoder sind unverändert. Die Ordner-Einstellungen wurden in V0.12.9 gezielt vereinfacht.
- Neu: automatisierte Hosttests mit allen sieben Installationsstrukturen, Root-Migration und Mock-FTP-Covertransfer.

## Übernommene Funktionen und Verhalten der V0.12.7


- Linke Spieleliste und synchroner Coverfächer bleiben. Hauptcover 256 × 358 statt 308 × 432; darunter kompakte Spielinformationen und drei Verlaufskarten.
- Verlauf enthält ausschließlich die bereits von BGH gespeicherten angenommenen Startanforderungen, keine erfundenen Spiele. Eine angenommene Startanforderung beweist noch keine erfolgreiche Emulation. Leere Plätze sind beschriftet. Touchpad fokussiert die Karten, links/rechts wählt, X öffnet die Details, oben/Kreis verlässt die Karten.
- Transparente Verlaufskarten und Hauptflächen mit Schatten, feinen Kanten und Lichtreflexen. Eigene Konsolensilhouetten für PS5, PS4, PS2, Switch und Xbox 360; keine Smartphone-Platzhalter.
- Vier Controllerplätze: Anmeldung und scePadReadState werden einmal pro Sekunde abgefragt. Weiß = verbunden, grau = nicht verbunden, ? = Abfrage nicht möglich. Keine erfundenen Akkustände. Die Anzeige entspricht den Benutzerplätzen. Bereits vom UI geöffnete Handles werden niemals geschlossen. Zusätzliche Handles gehören nur dem Monitor.
- Quadrat in PS2-/Switch-/Xbox-Details: experimenteller Systemaufruf zum Öffnen des jeweiligen Emulator-Menüs. Keine ROM-Übergabe, keine Änderungen an Emulator-Dateien und kein falscher Eintrag in den Spielverlauf.
- PS5-Startfehler nennt jetzt Titel-ID und tatsächlichen Rückgabecode. Die pauschale Behauptung „Titel muss registriert sein“ wurde entfernt. Kein nachgewiesener Fix für 0x8094000F.
- Xbox-Cover: zusätzliche Bilder neben der ISO/XEX; dynamische Suche nach tatsächlich per FTP aufgelisteten PPSA50011_<Kennung>-Sandboxverzeichnissen; dort JPGs in download0/xbox360ps5/covers und PNG-Spielicons in library. Der library-Dateiname folgt exakt dem 64-Bit-Hash des PS5X360-Quellcodes, einschließlich dessen besonderem Startwert.
- PS2-/Switch-Hashverfahren, Decoder und FTP-Transferlogik bleiben unverändert. Der funktionierende V0.12.4-Dateilesefix bleibt erhalten.

## Was noch nicht bestätigt ist

Dieses ZIP enthält Quellen, KEINE neu auf der PS5 getestete eboot.bin. GitHub Actions muss das Paket bauen.

Xbox: Die ISO wird bereits identifiziert (im Nutzerfoto 4D53082D). Der Screenshot beweist keine Erreichbarkeit des privaten Emulator-Caches. Ein Sandboxpfad ist nur nutzbar, wenn er tatsächlich gemountet und für den bestehenden FTP-Dienst sichtbar ist. BGH mountet oder extrahiert keine download0.dat. Falls kein Pfad sichtbar ist, bleibt die Coveranzeige leer. Eingebettete Paketbilder werden nicht neu extrahiert. Es werden keine Onlinecover heruntergeladen.

PS5: sceSystemServiceLaunchApp wird auf der Konsole mit 0x8094000F abgelehnt. Der konkrete Auslöser ist nicht nachgewiesen. Weder Registrierung noch Jailbreak-Einstellungen werden verändert. Ein Emulatormenü kann am selben Systemzugriff scheitern; deshalb ist der Knopf ein Test.

Switch: ProsperoEden ParseForwardedArgs unterstützt --rom und --exit-after-game. BGH besitzt bereits einen Client für einen externen Startdienst. Ohne laufenden passenden Dienst funktioniert dieser Weg nicht; V0.12.9 installiert keinen Dienst. Kein automatischer Fallback, der einen zweiten Start auslösen könnte.

PS2: Aktuelle PS5SX2-Quellen besitzen versionsabhängige Disc-Signaldateien unter /data/PCSX2/logs. Diese werden bewusst nicht angelegt oder verändert. Das Öffnen des Emulator-Menüs benötigt keine solchen Dateien. Ein unmittelbarer ROM-Start ist hier weiterhin nicht implementiert.

## Nachweise und Quellen (16.10.2026)

- Xbox: BrinooTk/PS5X360, Commit ad628f65ed460416c45103f4867053c5086eab1d:
  src/covers.cpp (CoverFile, ReadIsoTitleId), src/launcher.cpp (CacheKey, Scan), tools/console.py (game_logs).
  https://github.com/BrinooTk/PS5X360/tree/ad628f65ed460416c45103f4867053c5086eab1d
- PS2: Swordpdf/PS5SX2, Commit b4cf55e87c34e8a2a1cd9302df4373e5a94d28f4:
  ps5/sce_sys/param.json, ps5/coreorbis/main-boot.cpp, ps5/disclaunch/EMBEDDING.md.
  https://github.com/Swordpdf/PS5SX2/tree/b4cf55e87c34e8a2a1cd9302df4373e5a94d28f4
- Switch: blackbearreloaded/ProsperoEden, Commit e2263cd74150ddceca889aaf53e65aa6d669548d:
  headless/forwarded_launch.h, third_party/ps5_pad.hpp.
  https://github.com/blackbearreloaded/ProsperoEden/tree/e2263cd74150ddceca889aaf53e65aa6d669548d
- UI bleibt auf bbe19054ee2e7ec069254775273aeb8846c4bd3e festgeschrieben.

## Prüfung vor Auslieferung

Die neue Version muss mit `pytest tests/`, nativer GUI-Host-Syntaxprüfung und dem PS5-Build über GitHub Actions überprüft werden. Die Tests sind KEIN PS5-Hardwaretest. Der genaue Teststatus wird im Übergabebericht außerhalb des Projekts dokumentiert.

Nur BGH-eigene Daten werden geschrieben. Keine Änderung an Spielordnern, Emulator-Einstellungen, Spielständen, ShadowMount oder Jailbreak-Konfiguration.


## Übernommenes V0.12.7 LAKE Design
Neues eingebautes Landschaftshintergrundbild (`assets/premium_lake_background.png`), Sidebar-Plattformnavigation, breite Bibliotheksliste, Cover-Fächer rechts und Controller-Status in der Kopfzeile. Die bekannten PS5-/PS2-/Switch-Coverroutinen bleiben unverändert. Dies ist ein neuer GitHub-Quellcode-Build, kein auf PS5 bestätigtes UI. Aktive Apps/Emulatoren werden aus Alle Spiele ausgeblendet und unter Emulatoren (intern weiterhin Homebrew) eingeordnet.

## V1.0.5 – Neue Einstellungsstruktur

- **Spieleordner:** vorhandene Pfade, Ein/Aus, Hinzufuegen, Editieren und aktiver Scan bleiben erhalten. L2/R2 filtert hier die Plattformen.
- **Darstellung & Verhalten:** vorbereitete, klar als zukuenftig markierte Bereiche fuer Sprache und Emulator-Startmodi. In V1.0.5 sind diese bewusst nicht schaltbar.
- **System & Hilfe:** Controller-Tasten, Versionsanzeige und vorhandene Verlauf-/Speicherdiagnostik.
- **L1/R1:** nur im Einstellungsmenue die drei Tabs; ausserhalb unveraenderte Plattformnavigation.
- **Keine Migration:** `preferences.bin`, `paths.user.conf`, bisherige Scanpfade, Startlogik und Verlauf werden nicht veraendert.

**Neu in V1.0.9:** Listen-Header ohne äußere Kapsel, ungerahmter Plattformtext, Fokus-Glow, unbekannte Batterieanzeige ausgeblendet, echte PlayStation-Glyphen im Einstellungsfooter und Emulator-Menüstart direkt aus der Homebrew-Kategorie mit verifizierten Titel-IDs. Keine Änderung am ULGP-Verlaufsschema.


## V1.1.0 – Neues Branding und Musik

- Das neue BGH-Alpenemblem ersetzt das Launcher-Logo und das PS5-Home-Icon. Das schmale Schriftlogo im oberen Launcherbereich wurde passend angelegt.
- Der gesamte Cover-Fächer wurde um 42 virtuelle Pixel nach rechts verschoben, damit er weniger in die Liste „Meine Spiele“ hineinragt.
- Unter **Options → Darstellung & Verhalten** gibt es nun **Hintergrundmusik – Musik AN / Musik AUS**. Die Entscheidung wird unabhängig vom ULGP-Verlauf und den bisherigen Einstellungen prüfsummengeschützt in `music.bin` gespeichert.
- Der eingesandte MP3-Track wird für den **bereits vorhandenen** Vorbis-Musikplayer in ein 48-kHz-Stereo-OGG umgewandelt. Der BGH-Track wird ohne Titelwechsel in Dauerschleife abgespielt; das Deaktivieren schaltet den Musik-Bus stumm.
- **Urheberrecht:** Die Musik stammt aus *Ecco: The Tides of Time*. Für eine **öffentliche GitHub-Veröffentlichung** muss die Lizenz/Freigabe der Aufnahme und Komposition geklärt werden. Ohne Erlaubnis nur privat verwenden und den Audiotrack nicht öffentlich verteilen.


## V1.1.0 – Plattformregister und Emulatorverwaltung

Drei Einstellungs-Tabs: Emulatoren & Spielordner, Darstellung & Verhalten, System & Hilfe. Die alte separate Spieleordner-Kategorie ist entfallen; konfigurierte Scannerpfade und ihre AN/AUS-Werte bleiben erhalten.
Im Emulator-Tab: Hoch/Runter auswählen, X aktivieren/deaktivieren, L2/R2 die Plattform
in der Gesamt-Reihenfolge verschieben, Dreieck Ordnerbrowser, Touchpad Pfadeditor.
Die seitliche Plattformliste entsteht dynamisch aus aktivierten Profilen, die mittlere
Plattformliste ist scrollbar. Die feststehenden Elemente „Alle Spiele“, „Emulatoren“,
„Favoriten“ und „Zuletzt“ bleiben an ihren Positionen.

Änderungen werden nur in der eigenen BGH-Speicherung `platforms.bin` (zwei
prüfsummengeschützte Kopien) gesichert. `preferences.bin` und bestehende Scanordner
werden nicht migriert. Die ursprünglichen fünf Plattformen bleiben standardmäßig
aktiv und in unveränderter Reihenfolge. PS1/PSP haben nur Scanfunktionen, **keine
bestätigte Startschnittstelle**. PS3, Original Xbox, Dreamcast, GameCube und Wii
sind **Vorlagen ohne Scanner und ohne Emulator-Startbackend**. Das Registrieren
eines Profils erzeugt keine Emulatorunterstützung.

## V1.1.1 – Bedienung der universellen Plattformverwaltung

- Options → Emulatoren: PS5 und PS4 gehören nun zur globalen Sortierung (auch wenn sie keine Emulatoren sind).
- Hoch/Runter wählt Plattform, X aktiviert/deaktiviert sie. Auch Plattformen ohne Scan-Backend dürfen in der Seitenleiste erscheinen.
- L2/R2 verschiebt die ausgewählte Konsole in der *gemeinsamen* Reihenfolge aller Plattformen; Sortierung bleibt nach Neustart erhalten.
- Quadrat klappt die markierte Plattform auf. Unter der Plattform stehen die installierte Emulator-Anwendung bzw. bestätigte Titel-ID, ein optionaler Installationspfad und die Liste der Spielordner.
- Im aufgeklappten Eintrag wählt Hoch/Runter Anwendung, vorhandene Spielordner und die Plus-Zeile. X und Touchpad öffnen direkt den manuellen Pfadeditor (kein blockierter POSIX-Root-Browser). L3 schaltet bei einem vorhandenen unterstützten Scannerpfad dessen Einzel-Scan AN/AUS. Dreieck entfernt nur eine markierte Spieleordner-Verknüpfung – niemals die Spiele selbst.
- Bis zu 16 Benutzer-Spielordner pro Plattform in `platforms.bin` Format ULGR|2 mit CRC und atomarem Schreibvorgang. Vorherige ULGR|1-Pfade werden beim Laden übernommen; bestehende `paths.user.conf`-Scanroots bleiben maßgeblich für bekannte Scanner.
- PS3, Xbox Original, Dreamcast, GameCube und Wii erhalten nur vorbereitete, sichtbare Profile und monochrome Icons, **keine erfundenen Start-Backends**.
- Bereits bekannte PS5/PS4/PS2/Switch/Xbox360-Spiele bleiben in ihren bisherigen Launchpfaden. Favoriten, Recent, Music, Cache und bestehende Hintergrundgrafik bleiben unverändert.


## V1.1.3 – Drei Tabs und Ordnerdirekteingabe

- Emulatoren & Spielordner vereinigt die Plattformverwaltung und die bisherigen einzelnen Spieleordner. PS5 und PS4 bleiben sortierbar.
- X auf `+ Weiteren Spieleordner hinzufuegen` öffnet unmittelbar den Pfadeditor; X auf einem bestehenden Ordner bearbeitet den Eintrag. Touchpad bietet dieselbe direkte Pfadeingabe. Dreieck entfernt nur die Verknüpfung.
- L3 auf einem vorhandenen Scannerordner aktiviert/deaktiviert den einzelnen Pfad wie bisher im alten Tab. Sein Status wird neben dem Pfad angezeigt, und der Zustand bleibt in der bisherigen user_paths-Datei gespeichert.
- Das 512x512 Dashboard-App-Icon nutzt die Fläche stärker aus, ohne Verzerrung. PS4-/Xbox-360-Icons sind vom äußeren Alpha-Artefakt bereinigt.
- Die alte Funktion zum Folder-Browsing bleibt intern vorhanden, wird aber nicht mehr für X beim Hinzufügen aufgerufen (POSIX-Berechtigungen auf / können auf der PS5 den Browser verhindern).


## V1.1.3 – PS5-USB-Cover-Nachsuche (korrigiertes Paket)

- Auch wenn der POSIX-Scan ein externes PS5-Spiel in `/mnt/usb1/homebrew/PPSAxxxxx-app0` erfolgreich findet, kann BGH jetzt das Cover **über den bereits laufenden, rein lesenden FTP-Dienst** direkt im Spielordner nachladen. Der alte Fallback auf `/user/appmeta/<Titel-ID>/icon0.png` bleibt bestehen.
- Kandidaten für PS5: `cover.png`, `Cover.png`, `folder.png`, `poster.png`, `boxart.png`, `front.png`, `icon0.png`, `sce_sys/icon0.png`, `sce_sys/ICON0.PNG`. Nur vorhandene, validierte PNG-Dateien werden in BGHs privaten Covercache übernommen; Spieldateien werden nie verändert.
- Ein neuer privater Bibliotheks-Cache (`library_v113_ps5_cover.bin`) erzwingt einmalig eine neue Suche nach dem Update. `library_v129.bin` bleibt für einen Rückwechsel unberührt.
- Wenn weder der Spielordner noch die App-Metadaten ein lesbares Bild liefern, bleibt „Kein Cover“ korrekt: BGH erfindet keine Spielcover.
- Scan- und Launchpfade, Verlauf, Musik, Plattformreihenfolge und Cover-Laderate werden nicht geändert.


## V1.1.4 – USB-Scan-Reparatur (sauber aus V1.1.3)

Dieser Build basiert auf dem V1.1.3 PS5-USB-Cover-Fix-Test-Hotfix und integriert ausschließlich den USB-R1-Scannerfix sowie versionsbedingte Anpassungen von Anzeige, CI, Release-Pruefungen und Katalogdatei. Die frueher fehlgeschlagenen V1.1.4-SDK-/Python-/UI-Kit-Buildexperimente sind **nicht** enthalten.

* Fehlende PS5-Cover werden beim erneuten Scan priorisiert und mit eigenem FTP-Abrufbudget untersucht.
* PS5-Container `.ffpfsc` und `.exfat` werden in USB-Root-Scans katalogisiert, **nicht** automatisch gemountet oder gestartet.
* Neuer Katalogcache `library_v114_usb_scan_repair.bin`, alte Caches und Benutzereinstellungen bleiben erhalten.
* Der uebernommene native GitHub-Build erfordert weiterhin erreichbare gepinnte Abhaengigkeiten; erfolgreicher nativer PS5-Build und Konsolentest stehen aus.
* Branch: `bgh-v1.1.4-usb-rebuild`, Ausgangsbranch: `bgh-v1.1.3`.
