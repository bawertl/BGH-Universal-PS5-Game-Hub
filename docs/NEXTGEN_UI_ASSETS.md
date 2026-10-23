# BGH NextGen UI – native assets

The UI is rendered in the existing PS5 Homebrew UI kit at a 1920 × 1080 logical resolution.
No mockup is embedded as the UI and none of the working BGH game-launch backends is changed.

- **Landscape:** existing `assets/premium_lake_background.png`, preferred size 3840 × 2160, 16:9; 1920 × 1080 minimum.
- **BGH crest/crown:** optional future transparent `assets/bgh_crest.png`, 512 × 512 or 1024 × 1024 RGBA. **Not generated in this update**; original BGH emblem remains until the user approves an asset.
- **Wordmark:** existing `assets/bgh_wordmark.png`; suggested transparent 1536 × 256 RGBA.
- **Controller status:** existing `assets/ui/controller.png`, displayed at 42 × 42 logical pixels; source at least 128 × 128.
- **Platform icons:** existing `assets/platforms/*.png`, each at least 96 × 96 RGBA.
- **Game covers:** original user artwork/cache preserved; target 512 × 768 for portrait, with contained sizing for native square and landscape artwork.
- **Reflection:** computed from the *same* already-decoded game texture using reversed UV strips with diminishing alpha; no extra images and no GPU readback.
- **Focus:** existing DrawList glow and rounded outline, cyan (#00A2FF / #00E5FF), avoiding new GLSL code or build dependencies.
- **Disk widgets:** `statvfs` in-memory telemetry at most every 30 seconds, and an honest unavailable state if the PS5 sandbox denies access.

This is an **experimental UI branch**, not a replacement for the field-tested BGH V1.1.9.
