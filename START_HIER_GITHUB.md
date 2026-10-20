# BGH V1.1.4 – PS5 USB-Cover- und Container-Scanner-Reparatur

**Funktionierende Grundlage:** BGH V1.1.3, bestehender GitHub-Branch `bgh-v1.1.3`. Nicht veraendern.

**Neuer unabhaengiger Branch:** `bgh-v1.1.4-usb-rebuild` (NEU von `bgh-v1.1.3` erstellen, NICHT vom fehlgeschlagenen `bgh-v1.1.4`).

**GitHub-Artefakt:** `BGH-V1-1-4-USB-R1-PPSA99079-TEST`
**PS5-Paketname:** `BGH_BawertLs_Game_Hub_V1_1_4_PPSA99079_PS5_TEST.zip`
**PS5-Installationsordner:** `PPSA99079` (unveraendert).

1. Zuerst einen neuen Branch `bgh-v1.1.4-usb-rebuild` aus dem nachweislich funktionierenden `bgh-v1.1.3` anlegen.
2. Das GitHub-Update-ZIP in den REPOSITORY-ROOT hochladen. Es hat keinen uebergeordneten Projektordner.
3. GitHub Actions muss auf `bgh-v1.1.4-usb-rebuild` vollstaendig GRUEN sein. Bei Fehlern **nicht** die laufende PS5-App ersetzen.
4. Vor dem PS5-Update eine Backup-Kopie der derzeit funktionierenden `PPSA99079`-Installation anfertigen.
5. Erst nach erfolgreichem Build installieren und die Cover von Hellraiser, Pistol Whip, Light Brigade, Valor Mortis und den PS5-USB-Root-Scan pruefen.
6. Neu erkannte `.ffpfsc`/`.exfat`-Dateien werden nur angezeigt, nicht ungeprueft gestartet. Favoriten, Einstellungen und alte Katalog-Caches werden nicht geloescht.

Hinweis: Das upstream UI-Kit und PS5-OpenGL-SDK sind externe Buildabhaengigkeiten. Diese Quellen koennen trotz korrektem Projektcode unerreichbar sein. Dieses Paket nimmt keine ungetesteten Aenderungen an der SDK-Buildkette vor.
