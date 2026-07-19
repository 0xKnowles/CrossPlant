# CrossPlant 🌱

> **A full project website (same content as this README, browsable and searchable) lives in
> [`docs/`](docs/) — open [`docs/index.md`](docs/index.md) locally, or enable GitHub Pages on this
> repo (Settings → Pages → Deploy from branch → `/docs`) to serve it at
> `https://0xknowles.github.io/CrossPlant/`.**

**CrossPlant** is an open-source custom firmware for the **Xteink X3 and X4** e-ink readers. It merges the stable, high-performance reading base of **[CrossInk](https://github.com/uxjulia/CrossInk)** with a fully re-engineered virtual-plant companion system descended from **[CrossPet](https://github.com/trilwu/crosspet)** — so the healthier your reading habits, the healthier (and more evolved) your plants become.

<table>
  <tr>
    <td align="center" width="33%">
      <img src="./bmp/mon-4.bmp" width="140" alt="Prized Monstera sprite" /><br/>
      <sub><b>Monstera</b> — Prized</sub>
    </td>
    <td align="center" width="33%">
      <img src="./bmp/begonia-4.bmp" width="140" alt="Prized Begonia sprite" /><br/>
      <sub><b>Begonia</b> — Prized</sub>
    </td>
    <td align="center" width="33%">
      <img src="./bmp/alo-4.bmp" width="140" alt="Prized Alocasia sprite" /><br/>
      <sub><b>Alocasia</b> — Prized</sub>
    </td>
  </tr>
</table>

---

## Where CrossPlant comes from

CrossPlant is a fork-of-forks. Each layer kept what worked from the one below it:

| Layer | What it is | What it contributed |
| --- | --- | --- |
| [crosspoint-reader](https://github.com/crosspoint-reader/crosspoint-reader) | The original open-source Xteink reader firmware. | Core reading engine, EPUB/TXT/XTC rendering, file browser, WiFi sync, the ESP32-C3 platform layer. |
| [CrossInk](https://github.com/uxjulia/CrossInk) | A personal fork of crosspoint-reader focused on typography and lightweight stats. | Lexend Deca / Bitter reader fonts, Dashboard & Minimal themes, all-time and nearby reading-stats sync, bookmarks, clippings, KOReader sync, and dozens of stability fixes on the ESP32-C3's ~380 KB RAM budget. |
| [CrossPet](https://github.com/trilwu/crosspet) | A community fork that bolted a Tamagotchi-style pet onto crosspoint-reader. | The original concept: a companion that hatches, has decaying vitals, and evolves based on how much you read. |
| **CrossPlant** (this repo) | Ports CrossPet's pet mechanics onto the current CrossInk base, then rebuilds them from the ground up as a *plant*-growing game with deep reading integrations. | Everything below. |

If you're looking for general reader features (fonts, EPUB rendering, sync, themes), see [Reader base](#-reader-base-from-crossink) — that's inherited from CrossInk almost unchanged. Everything under **My Plants** is CrossPlant's own layer on top.

> **Status:** actively developed, expect rough edges. Bug reports and feature ideas are welcome — see [Contributing](#contributing).

---

## 🌿 My Plants — the virtual plant system

Your plants aren't idle decoration — their stats decay while you're awake and are replenished almost entirely by reading. Put the device to sleep and they rest until you wake it again.

### Grow up to 3 plants at once

Start with a single growing plot, then unlock up to **3 simultaneous plots** — one of each species — through the Dew Shop. Plot 2 and Plot 3 are priced as genuine high-value purchases (800 and 1,200 $Dew) above every other shop item, so they're a real long-term goal, not an impulse buy.

- **Front bezel buttons** switch which plot is active.
- **Side volume-rocker buttons** drive the plant action menu.
- Currency, water/fertilizer stock, shop upgrades, and your reading streak are **shared** across all plots.
- Each plot's vitals, growth stage, and evolution progress independently, based on its own care and reading history.

### Species & growth

Choose a species when you hatch a new plant — **Monstera**, **Begonia**, or **Alocasia** — each with hand-crafted, hand-dithered 1-bit pixel art at every growth stage, and its own growth vocabulary matched to how that plant is actually propagated:

<table>
  <tr>
    <td align="center"><b>Monstera</b></td>
    <td align="center">Seed → Sprout → Juvenile → Mature → Prized</td>
  </tr>
  <tr>
    <td align="center"><b>Begonia</b></td>
    <td align="center">Cutting → Rooted → Leafing → Mature → Prized</td>
  </tr>
  <tr>
    <td align="center"><b>Alocasia</b></td>
    <td align="center">Corm → Pup → Sprouting → Mature → Prized</td>
  </tr>
</table>

<p align="center">
  <img src="./bmp/begonia-1.bmp" width="100" alt="Begonia cutting" />
  <img src="./bmp/begonia-2.bmp" width="100" alt="Begonia rooted" />
  <img src="./bmp/begonia-3.bmp" width="100" alt="Begonia mature" />
  <img src="./bmp/begonia-4.bmp" width="100" alt="Begonia prized" />
  <br/>
  <sub>Begonia's full growth progression: Rooted → Leafing → Mature → Prized</sub>
</p>

At the two branching stages, your reading *quality* — not just quantity — determines which variant you get:

- **Scholar branch** — active reader: a 7+ day streak, at least one finished book, and well above the page threshold for that stage.
- **Wild branch** — infrequent, inconsistent reading.
- **Default branch** — everything in between.

### Vitals & care

Four vitals decay over time and are restored by five hands-on actions:

| Vital | Action | Notes |
| --- | --- | --- |
| Moisture | **Water Plant** | Consumes a charge from your Water Jug (3/3 capacity, refills for free). |
| Sunlight | **Shade Plant** | Also boosted passively by sunny/snowy weather (see below). |
| Health | **Weed Pot** | Clears pest/neglect penalties. |
| Nutrients | **Fertilize** | Consumes a charge from your Fertilizer stock (3/3 capacity, restocked for 30 $Dew). |

A fifth action, **Tend Plant**, is general upkeep that also counts toward the daily "Tend 5 Times" quest. Height (cm) climbs automatically as the plant matures and is shown alongside its stats everywhere.

### Dew Drops & the Dew Shop

Reading earns **Dew Drops** ($Dew) — the in-game currency — automatically as you turn pages, finish chapters, and finish books. Spend them in the **Dew Shop** on upgrades that change decay math rather than just cosmetics:

| Item | Cost | Effect |
| --- | --- | --- |
| Premium Sprayer | 300 $Dew | +10 Sunlight/Happiness on misting. |
| Moss Pole | 250 $Dew | Halves Sunlight decay. |
| Self-Watering Pot | 400 $Dew | Halves Moisture decay. |
| Slow-Release Fertilizer | 500 $Dew | Auto-regenerates +1 Nutrient every 2 hours. |
| Greenhouse Cover | 650 $Dew | Halves Health decay. |
| **Growing Plot 2** | **800 $Dew** | Unlocks a second simultaneous plot. |
| **Growing Plot 3** | **1,200 $Dew** | Unlocks a third simultaneous plot. |

### Real-time weather sync

When connected to WiFi, CrossPlant periodically geolocates the device (via IP) and checks current conditions through Open-Meteo, applying a small passive hourly boost that matches the weather: **Sunny** → Sunlight, **Rainy** → Moisture, **Cloudy** → Nutrients, **Snowy** → Health. The current condition and its bonus are shown on both the main plant card and the sleep screen.

### Daily quests

Six quests reset every day and pay out in Dew Drops on completion: Read 30 Pages, Tend 5 Times, Water 3 Times, Speedy Reader (15 pages in one sitting), Night Owl (10 pages after 9 PM), and Streak Saver (maintain a 3-day streak).

### Herbarium

A discovery log tracks every stage of every species you've ever grown — 3 species × 4 visible stages = 12 total entries — labeled with each species' own growth vocabulary, so you can see your collection progress at a glance.

---

## 📖 Reading experience

### Dashboard integration

The Dashboard home theme's footer shows a live mini-sprite of your active plant alongside its vitals, sized to fill the space next to your current book and reading stats — no need to leave the home screen to check in on it.

### Plant sleep screen

Putting the device to sleep with an active plant switches to **"CrossPlant Sleeping"** — a dedicated card-based standby screen: a large plant portrait with its name, growth stage, and species; a stats column (age, moisture, sunlight, nutrients, health, and the actual local time you fell asleep); a footer showing active passive boosts and current weather; and — space permitting — a Reading Stats card with bold section headers, a cover of the last book you read, and your total time/books/pages read plus current streak.

### Boot screen

Boot now greets you with a 3×3 showcase grid — one column per species — at a curated mix of growth stages, instead of a single logo.

<p align="center">
  <img src="./Seed.bmp" width="90" alt="Seed" />
  <img src="./bmp/mon-4.bmp" width="90" alt="Monstera prized" />
  <img src="./bmp/begonia-2.bmp" width="90" alt="Begonia leafing" />
  <img src="./bmp/alo-3.bmp" width="90" alt="Alocasia mature" />
</p>

### Global quick actions

Long-press shortcuts for the Back and Confirm/Menu buttons (Settings → Controls) — screenshot, reading stats, sleep, file transfer, virtual pet, and more — now work from **anywhere in the app**, not just while a book is open.

### 📚 Reader base (from CrossInk)

Everything CrossInk brought to crosspoint-reader carries through unchanged:

- **Typography**: [Lexend Deca](https://fonts.google.com/specimen/Lexend+Deca) and [Bitter](https://fonts.google.com/specimen/Bitter) reader fonts, chosen for fluency and e-ink legibility, plus a limited emoji/symbol glyph set.
- **Themes**: Dashboard, Minimal, and the original reader theme, each with matching sleep-screen modes.
- **Reading aids**: Bionic Reading, Guide Dots, Force Paragraph Indents, adjustable line spacing, and per-book render modes (`Default` / `Balanced` / `Light`) for memory-heavy EPUBs.
- **Stats & sync**: Total books read, reading time, sessions, pages turned, pace — synced between devices, and now also all-time streaks feeding directly into plant evolution.
- **Library tools**: Bookmarks, clippings/highlights with `My Clippings.txt` export, recent books grid, KOReader sync, and OPDS catalog browsing.
- **Reliability**: Extensive low-memory fallback handling across EPUB parsing, layout, and caching — this firmware runs on an ESP32-C3 with no PSRAM and ~380 KB of usable RAM.

See [CHANGELOG.md](./CHANGELOG.md) for the full, version-by-version history of every change inherited from CrossInk plus everything CrossPlant has added since.

---

## Hardware

| | Xteink X3 | Xteink X4 |
| --- | --- | --- |
| Display | 528×792 e-ink (portrait) | 800×480 e-ink (landscape) |
| Clock | Dedicated DS3231 RTC (accurate across sleep) | Internal ESP32-C3 RTC (drifts during deep sleep) |
| MCU | ESP32-C3, single-core RISC-V @ 160 MHz, no PSRAM | Same |

Both devices ship with roughly 380 KB of usable RAM, so CrossPlant — like CrossInk before it — leans on the SD card for caching rather than holding everything in memory. See [Data Cache](./docs/data-cache.md) for the `.crosspoint` layout.

---

## Building & installing

CrossPlant uses [PlatformIO](https://platformio.org/) for building and flashing. There are two firmware size variants due to flash constraints — `tiny` (compact font set) and `xlarge` (largest point sizes only) — plus a `default`/`debug` env for local development and a `simulator` env for testing without hardware.

```sh
# Build + flash directly over USB-C
pio run -e tiny --target upload

# Build only (e.g. to grab firmware-tiny.bin for the web installer)
pio run -e tiny
```

Prebuilt `firmware-*.bin` artifacts are produced automatically by CI on every push — see the [Actions tab](https://github.com/0xKnowles/CrossPlant/actions) for the latest build of any branch. Packaged GitHub Releases with ready-to-flash `.bin` files are coming soon. To flash a downloaded `.bin`, use the web installer or `esptool` — see [Installation](./docs/installation.md) for both paths.

### Simulator

To iterate without flashing hardware, build the [device simulator](https://github.com/uxjulia/crosspoint-simulator), which renders the e-ink display in an SDL2 window:

```sh
pio run -e simulator
.pio/build/simulator/program
```

See [Simulator](./docs/simulator.md) for platform setup notes, keyboard controls, and cache-clearing tips.

---

## Documentation

- [User Guide](./USER_GUIDE.md) — hardware controls, navigation, and reading features.
- [Installation](./docs/installation.md) — web installer and command-line flashing.
- [Controls](./docs/controls.md) — remapping bezel buttons and shortcuts.
- [Data Cache](./docs/data-cache.md) — the `.crosspoint/` on-SD-card layout.
- [File Formats](./docs/file-formats.md) — binary cache format details.
- [Simulator](./docs/simulator.md) — running the SDL2 device simulator.
- [Web Server Guide](./docs/webserver.md) / [Endpoints](./docs/webserver-endpoints.md) — the on-device file-transfer web server.
- [Troubleshooting](./docs/troubleshooting.md)
- Full doc index: [docs/index.md](./docs/index.md)

---

## Contributing

CrossPlant inherits its scope philosophy from crosspoint-reader and CrossInk: this is a *reader* first, so contributions that improve reading — typography, rendering, library management, legibility — are always welcome. Plant-system contributions are equally welcome as long as they stay reading-driven (no idle-clicker mechanics, no features that only make sense with the device left plugged in and awake). See [SCOPE.md](./SCOPE.md) before proposing something large, and [GOVERNANCE.md](./GOVERNANCE.md) for how the project is run. If you're unsure whether an idea fits, open a Discussion first.

For local setup, code verification, and PR guidelines, start with [docs/contributing/getting-started.md](./docs/contributing/getting-started.md).

---

## Credits

- [crosspoint-reader](https://github.com/crosspoint-reader/crosspoint-reader) by Dave Allie and contributors — the original firmware this entire lineage is built on.
- [CrossInk](https://github.com/uxjulia/CrossInk) by uxjulia — the typography-and-stats fork CrossPlant is directly based on.
- [CrossPet](https://github.com/trilwu/crosspet) by trilwu — origin of the reading-driven companion concept CrossPlant's plant system evolved from.

## License

MIT — see [LICENSE](./LICENSE).
