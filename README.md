# CrossPlant (CrossPet Fork🌿🐣

**CrossPlant** is an open-source custom firmware fork for the **XTEink X3** and **X4** e-ink readers. It merges the advanced typography, performance optimizations, and stable base of **CrossInk** with the virtual plant mechanics of **CrossPet**, adding a collection of enhancements, modern UI layouts, and deep reading integrations.

---

## 🐣 The Virtual Plant System (CrossPet Re-engineered)

CrossPlant features a fully integrated virtual plant companion that hatches from a seed and grows as you read. Your reading habits directly sustain your plant!

### Vitals & Care
*   **Decay & Needs**: Your plant's stats (Moisture, Happiness, Health, and Discipline) decay while you are awake. Under-watering and sickness will slowly deplete its Health.
*   **Waste & Cleanliness**: Feeding and watering increases weight and creates waste/weeds. Neglecting weeding makes the plant unhappy and sick.
*   **6 Daily Quests**: Every day, your plant has six interactive daily goals that reward you with **Dew Drops (DD)** upon completion.
*   **Evolution Branches**: Your plant evolves through stages: *Seed → Sprout → Sapling → Mature → Prized*. Depending on your reading streaks and completed books, it branches into one of three evolution variants:
    *   **Scholar** 🎓 (High streaks, active reader, books finished)
    *   **Balanced** ⚖️ (Default healthy cycle)
    *   **Wild** 🌲 (Infrequent reading, broken streaks)

---

## 🚀 Advanced Enhancements over CrossPet

CrossPlant overhauls the original pet system with premium features, modern layouts, and rich reading integrations:

### 1. Two-Column UI Overhaul 📱
The Virtual Plant dashboard features a space-efficient two-column layout:
*   **Left Column (Vitals & Status)**: Centered breathing plant sprite, active condition indicators, plant name/stage, compact progress bars for vitals (Moisture, Happiness, Health, Discipline), and your active **Dew Drops** balance.
*   **Right Column (Actions & Stats)**: Displays detailed **Reading Partner Stats** (page turns, sessions, streak details) and a scrollable actions menu (Water, Play/Tend, Fertilize, Shade, rename, etc.).

### 2. Dew Drops (DD) Currency & Shop 🛒
Read more to earn points and customize your plant:
*   **Real-time Reading Rewards**: Accumulate **Dew Drops (DD)** automatically:
    *   `+1 DD` per page turned
    *   `+100 DD` per book completed
    *   `+1 DD` per 30 seconds of reading duration (synchronized in real time)
    *   `+5 DD` per new reading session started
    *   `+10 DD` bonus for completing a chapter (synchronized in real time)
*   **Dew Drops Shop**: Access the shop to spend points on items:
    *   **Treat Box** (`20 DD`): Restores Moisture and Health.
    *   **Reading Toy** (`50 DD`): A passive item that halves your plant's happiness decay rate.
    *   **Round Glasses** (`100 DD`): Cosmetic accessory.
    *   **Wizard Hat** (`150 DD`): Cosmetic accessory.
    *   **Crown** (`300 DD`): Premium cosmetic accessory.
    *   **Scarf** (`120 DD`): Premium cosmetic accessory.

### 3. Cosmetic Equipment 🕶️🎩
Buy accessories in the Shop to customize your plant's appearance. Equipped items (Glasses, Wizard Hat, Crown, Scarf) render dynamically on top of the plant's pixel-art sprite at all scales.

### 4. Plant Diary Sleep Screen 📓
Sleep screen mode features a dual-pane **Plant Diary**:
*   The left half shows your sleeping plant with its name and stage.
*   The right half renders a ruled notebook diary summarizing the day's achievements (Age, pages read, tended count, general condition, and bedtime synced from the RTC).

### 5. Home Screen & Bezel Key Shortcuts 🕹️
*   **Dashboard Theme**: Displays a mini-sprite and stats (Moisture, Happiness, Health, Stage) in the homepage footer.
*   **Physical Settings Button**: Re-mapped on the Dashboard theme as a fast-track shortcut to open **My Plant** directly. The **Settings** menu is nested inside the Back-button overlay menu instead.
*   **Bezel Long-Press Shortcuts**: Hold down any bezel button (**Confirm, Left, or Right**) for 1 second to instantly jump to your plant screen from anywhere.

### 6. High-Fidelity SD Card `.bmp` Sprites 🖼️
In addition to the built-in 1-bit pixel art fallbacks, CrossPlant dynamically probes the SD card (`/.crosspoint/pet/sprites/`) for custom `.bmp` plant stage assets (e.g. `egg.bmp`, `hatchling.bmp`, `youngster.bmp`, `companion.bmp`, `elder.bmp`, `dead.bmp`, with variants/moods supported) and scales them cleanly to `144x144` or `24x24` for mini-sprites.

---

## 📖 Stable Reader Base Highlights (CrossInk)

- **Reader Typography**: Default fonts replaced with **Lexend Deca** (a research-backed sans-serif designed to improve reading fluency) and **Bitter** (a comfortable slab-serif optimized for digital screens).
- **Thicker Underlines & Strikethroughs**: Improved markup contrast on e-ink displays.
- **Section Breaks & Formatting**: Support for `<hr>` breaks, paragraph indents override, redaction styles, and tables.
- **Reader Modes**: Bionic Reading and Guide Dots toggle.
- **Stats & Syncing**: Sync reading progress and all-time statistics between two devices.

---

## 🛠️ Flashing & Installation

1.  Download the latest `firmware-default.bin` from the releases page.
2.  Connect your X3 or X4 via USB-C.
3.  Flash using the web installer or PlatformIO:
    ```sh
    pio run -e default --target upload
    ```

For detailed instructions, see [Installation](./docs/installation.md).

---

## 💻 Simulator

To test and develop without flashing hardware, compile the simulator which renders the e-ink screen in an SDL2 window. See [Simulator Guide](./docs/simulator.md).

---

## 📝 Documentation Index

*   [User Guide](./USER_GUIDE.md) - How to navigate and use the reader.
*   [Controls Guide](./docs/controls.md) - Remapping bezel buttons, short power click, and long-press actions.
*   [Developer Getting Started](./docs/contributing/getting-started.md) - Setup, code verification, and guidelines.
*   [Data Cache Layout](./docs/data-cache.md) - Understanding `.crosspoint/` and metadata storage.
