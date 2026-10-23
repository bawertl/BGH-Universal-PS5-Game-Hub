# BGH V1.1.9 – deklarative Emulator-Startprofile (experimentell)

BGH erkennt installierte PS5-Emulatoren anhand von `sce_sys/param.json`,
`eboot.bin` und profilabhängigen Dateien. Diese Erkennung allein bestätigt
**keinen** funktionierenden ROM-Direktstart.

Nur wenn ein bekanntes Profil eine **dokumentierte und getestete** Startmethode
beschreibt, kann BGH beim expliziten Druck auf X das ausgewählte Spiel an den
bereits laufenden PS5-Forwarder auf `127.0.0.1:10199` übergeben. Es werden
weder Hilfs-ELFs ausgeführt noch Emulator-Konfigurationen überschrieben.

## KB-RPCS3 (`PPSA99303`)

Der öffentliche Quellcode von KB-RPCS3 prüft beim App-Start `--boot <path>`.
Sein eigenes Game-Tile-System verwendet dasselbe Argument. BGH nutzt deshalb:

- Titel-ID: `PPSA99303`
- Argument 1: `--boot`
- Argument 2: `{game_app0}`
- Erlaubte Formate: `ps3_ordner`, `iso`

Der Platzhalter `{game_app0}` wandelt ausschließlich Pfade *innerhalb des
konfigurierten Installationsordners* um. Beispiel:

`/data/homebrew/PPSA99303/rpcs3/games/BLES01181-MeinSpiel`
→ `/app0/rpcs3/games/BLES01181-MeinSpiel`

Die genaue Titel-ID, das Profil und die Dateistruktur müssen zuvor auf der PS5
erkannt worden sein. Für externe PS3-Spielordner außerhalb des
Installationsordners wird dieser Startmodus **nicht** aktiviert.

Ob der vorhandene Forwarder die Argumente bei genau der auf der PS5 installierten
KB-RPCS3-Version korrekt übergibt, muss auf der echten Konsole getestet werden.

## Profile für weitere Emulatoren

BGH bringt die Grundstruktur für alle vorhandenen Konsolen-Plattformen mit.
Zusätzliche bekannte Emulatoren können über `/data/bgh/emulator_profiles.json`
als Profil definiert werden. Die mitgelieferten Profile liegen in
`assets/emulator_profiles.json`.

Optional kann ein Profil enthalten:

```json
"rom_launch": {
  "method": "forwarder_argv",
  "args": ["--game", "{game_path}"],
  "formats": ["iso"]
}
```

`--game` ist nur ein **Formatbeispiel**, kein nachgewiesenes Startargument
für irgendeinen konkreten Emulator. Es darf erst eingetragen werden, wenn
dessen Quellcode oder Schnittstellen-Dokumentation es bestätigt.

Erlaubte Platzhalter sind `{game_path}` (der von BGH erkannte absolute
Spielpfad) und `{game_app0}` (die oben beschriebene App-Ordner-Abbildung).
BGH baut eine Argumentliste, keine Shell-Zeile. Ohne gültiges Profil,
verifizierte Installation, passendes Spiel-Format oder erlaubten Pfad
bleibt der ROM-Direktstart gesperrt. Die separate Hauptmenü-Funktion im
Einstellungsbereich bleibt verfügbar.

Bekannte PS2-, Switch- und Xbox-360-Startwege haben Vorrang und bleiben
unverändert. Neue Emulatoren unterstützen nicht automatisch dasselbe
Startprotokoll; die Profile ermöglichen die Ergänzung **ohne C++-Änderung**,
sobald das jeweilige Protokoll belegt ist.
