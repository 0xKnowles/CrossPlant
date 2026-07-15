## [Unreleased]

### Added

- My Plants Dark Mode: Added a new "My Plants Dark Mode" toggle under Settings > Display that inverts the My Plants screen and the plant sleep screen, independent of the existing reader-only Dark Mode setting.

### Fixed

- Reader Dark Mode Now Global: The Settings > Reader > Dark Mode toggle now applies to every book. Dark Mode was previously stored per-book, so opening any book with saved reader settings silently reverted it to that book's stored value and the global toggle appeared to do nothing. Dark Mode is no longer bundled into per-book reader-setting snapshots; the in-reader "Toggle Dark Mode" quick action now updates the global setting too. Per-book font, margins, and other reader settings are unchanged.

## [v0.1.2] - 2026-07-13

### Added

- Multiple Growing Plots: Grow up to 3 plants at once, one of each species. Plot 2 (800 DD) and Plot 3 (1200 DD) unlock in the Dew Shop as high-value purchases. The front bezel buttons (previously "Previous"/"Next") switch the active growing plot, while the side volume-rocker buttons now drive the plant action menu. The active plot ("Plot X/Y") is shown under the plant sprite once more than one plot is owned. Currency, water/fertilizer stock, shop upgrades, reading streak, and quests remain shared across all plots, while each plot grows and evolves independently based on its own care and reading history.
- Boot Screen Showcase Grid: The boot screen now shows a 3x3 grid of plant portraits (one column per species: Monstera, Begonia, Alocasia) at a curated mix of growth stages, instead of a single plant logo. The top-left cell shows the seed art directly, since Monstera has no unique baked stage-1 sprite of its own (it's aliased to Begonia's).
- Water & Fertilizer Stock System: Introduced a stock mechanic for plant care where watering consumes charges from a Water Jug (capacity 3) and fertilizing consumes Fertilizer (capacity 3). Stock levels are dynamically displayed next to actions in the right-hand action list.
- Supplies Actions Section: Added a dedicated third section in the right-hand action menu containing Refill Water (Free), Buy Fertilizer (30 DD), and the Dew Shop.
- Plant Shop Passive Boosts: Replaced cosmetic shop accessories with functional plant tools: Premium Fertilizer (100 DD, refills all vitals and HP), Moss Pole (250 DD, halves Sunlight decay rate), Self-Watering Pot (400 DD, halves Moisture decay rate), Slow-Release Fertilizer (500 DD, automatically regenerates +1 Nutrient/discipline every 2 hours), Greenhouse Cover (650 DD, halves Health decay rate), and Premium Sprayer (300 DD, adds +10 Sunlight/Happiness bonus on misting).
- Herbarium Collection Log: Added a new "Herbarium" screen that displays discovery completion statistics (e.g., 4/12 discovered) and lists which of the 4 growth stages of the 3 plant types have been unlocked by the user. Discovery is tracked automatically across all plant grow cycles.
- Weather-Based Growth/Vitals Bonus: Periodically queries local weather conditions in the background when connected to WiFi (using Open-Meteo and ip-api geolocation) to apply weather-specific passive boosts (Sunny gives Sunlight boost, Rainy gives Moisture boost, Cloudy gives Nutrient boost, Snowy gives Greenhouse health boost), displaying current weather on the Status Card and Sleep screen.
- Gardening Overhaul: Transformed the "My Pet" feature into a tropical plant-growing game named "My Plants".
- Plant Types: Choose from three tropical species: Monstera, Begonia, and Alocasia.
- Plant Vitals & Actions: Replaced pet vitals and actions with Moisture (Water Plant), Sunlight (Mist Leaves / Move to Sun), Health (organic Pest Treatment), Nutrients (Fertilize), and Height.
- Dew Drops Currency: Changed Ink Points (IP) currency to Dew Drops (DD).
- Garden Shop: Replaced shop items with gardening-themed items: Premium Fertilizer (20 DD), Self-Watering Pot (50 DD), Cute Ladybugs (100 DD), Mini Umbrella (150 DD), Fairy Lights (200 DD), and Plant Ribbon (250 DD).
- Custom 1-Bit Plant Sprites: Hand-crafted 1-bit plant pixel art for all stages (Seed, Sprout, Sapling, Mature/Prized Monstera/Begonia/Alocasia, and withered twigs).
- Global Back/Confirm Long-Press: The long-press quick actions for Back and Confirm/Menu (Settings > Controls) now also work outside the book reader for actions that don't need an open book — Sleep, Screenshot, Reading Stats, File Transfer, Calibre Wireless, Join Network, Create Hotspot, Virtual Pet, and Browse Files. Previously these only fired while reading; on every other screen (Home, My Plants, Settings, etc.) holding Back or Confirm did nothing except the globally-wired Power button.

### Changed

- Direct Supplies Refilling: Water can now be refilled for free, and fertilizer can be purchased for 30 Dew Drops directly from the right-hand action list instead of entering the Dew Shop screen.
- Restructured Dew Shop: Restructured the Dew Shop to exclusively feature passive growth upgrades (Moss Pole, Self-Watering Pot, Slow-Release Fertilizer, Greenhouse Cover, Premium Sprayer), removing necessity supplies from the shop screen.
- Dynamic Reading-Based Quests: Replaced the daily Weed, Prune, and Fertilize quests with dynamic reading quests: Speedy Reader (15 pages read in one session), Night Owl (10 pages read after 9 PM), and Streak Saver (maintaining a reading streak of 3+ days).
- Improved Daily Quests UI/UX: Redesigned the Quests page to cleanly left-align mission titles and right-align progress bars / completed statuses, completely eliminating text overlapping.
- Cozy Sleep Screen Weather Info: Integrated the active weather condition and its bonus indicator into the Cozy Resting Diary on the Sleep Screen.
- Reverted and completely removed the experimental "Cover Pet" UI theme.
- Redesigned the main screen into a beautiful card layout with rounded borders: Top Left card for plant vitals, Bottom Left card for the plant sprite frame, and Right card for plant actions.
- Redesigned the sleep screen to display the sleeping/dormant plant in a cozy rounded frame on the left, and a detailed lined notebook diary page on the right.
- Updated all UI and reader themes (Base, Dashboard, Minimal, Lyra Carousel) to print DD currency, height, and moisture/sunlight vitals.
- Updated daily quests from "pets" to "tends".
- Enlarged the plant mini-sprite icon in the Dashboard theme's footer (from a fixed 20px up to 40px, sized to fill the same vertical space as the 3-line stats block beside it) so it reads more clearly, without growing the footer row or overlapping the pet name/stage/stats text (which still truncates dynamically to fit).
- Redesigned the Reading Stats card on the plant sleep screen: each stat now shows a bold caps label (e.g. "TIME READ") above a larger value instead of a single plain "Label: value" line, and rows are spread evenly across the card's full height instead of clustering at the top, removing the empty space that used to sit below them.
- Enlarged the plant sprite on both the plant sleep screen (220px -> 260px) and the "My Plants" screen (144px -> 192px).
- Renamed the plant sleep screen's title from "CrossPlant Dormant" to "CrossPlant Sleeping" (header and footer status label).
- Updated the boot screen's version footer to "Build v0.1.2".
- Species-Specific Growth Stage Names: Growth stage names now match each species' real propagation method instead of one generic set of terms. Monstera: Seed - Sprout - Juvenile - Mature - Prized. Begonia: Cutting - Rooted - Leafing - Mature - Prized. Alocasia: Corm - Pup - Sprouting - Mature - Prized. Applies everywhere a stage name is shown, including the Herbarium's per-species discovery list.

### Fixed

- Fixed Fertilize Action Behavior: Rewrote the fertilizing action so it always increases the plant's nutrients (discipline) stat instead of behaving as a pet discipline scolding mechanic, satisfying the nutrient attention call immediately.
- Daily Quests UI Overlapping: Redesigned the Daily Quests page layout to use a stacked two-line format, completely preventing overlapping text.
- Fixed UI alignment issues on the "My Plants" screen where section headers overlapped each other and reading progress text was shifted.
- Fixed Dew Drops (DD) currency not updating during reading sessions by dynamically calculating the plant's sleep state from the RTC time on page turns, resolving cases where the plant remained locked in a stale sleeping state.
- Removed duplicate DD counter from the top status bar in the reader, leaving the single counter near the task bar at the bottom.
- Fixed plant sleep and daily rollover schedules operating on UTC time instead of the user's configured local timezone, which caused plants to sleep during local daytime hours (e.g., afternoon/evening for Pacific time users).
- Fixed UI overlapping issues on the Dashboard Theme footer where the plant name/stage and 'Moisture'/'Health' text would collide on narrower screens by shifting the name/stage text closer to the mini-sprite icon and applying dynamic truncation.
- Added a "Daily Quests" menu option to the "My Plants" action menu to view detailed progress bars for all active daily missions.
- Fixed Cozy Frame box height on the sleep screen to prevent the bottom border from overlapping the plant stage text on X3 stacked layouts.
- Fixed the last-book cover on the plant sleep screen's reading-stats card rendering mostly black: it was generating a Dashboard-sized (296x444) thumbnail and then shrinking it down to the small card box, which re-samples already-dithered pixels. It now generates the thumbnail directly at the card's actual size, so dithering happens once at (close to) final resolution.
- Replaced remaining hardcoded "Pet" references on the sleep screen and plant details cards with "Plant" (e.g. "PET DIARY" -> "PLANT DIARY", "PET STATUS" -> "PLANT STATUS", and "PET ACTIONS" -> "PLANT ACTIONS").
- Fixed Settings being unreachable from the Home screen menu on the Classic, Lyra, Lyra 3 Covers, and Rounded Raff themes whenever a plant was alive: the Home menu's item counter used for Up/Down navigation didn't account for the "My Plants" row those themes add above File Transfer/Settings, capping navigation one item short of the list and silently making Settings (always the last row) impossible to select.
- Fixed the "REST TIME" shown on the plant sleep screen displaying raw UTC time instead of the wall-clock local time set in Settings > Clock UTC Offset. Every other on-screen clock (header, clock sync/offset settings, reading stats, clippings) already converted the RTC's UTC reading to local time; this one row was reading the RTC directly and skipping that conversion.
- Added support for loading custom species-specific `.bmp` plant images from the SD card (e.g. `alo-1.bmp`, `begonia-2.bmp`, `mon-3.bmp` under `/.crosspoint/pet/sprites/`) with automatic ordered Bayer dithering, and baked them directly into the firmware as dithered 1bpp fallbacks that center automatically inside their display bounding box without distortion or SD card requirements.
- Fixed a pre-existing simulator compilation error in `WifiSelectionActivity.cpp` by wrapping `WiFi.disconnect` parameters in a simulator check.
- Renamed the OS name from "CrossInk" / "CrossMerge" to "CrossPlant" across all boot screens, sleep screens, version footers, and system settings tabs.
- Updated the boot screen to display the custom Alocasia Mature (alo-3) graphic and aligned it with the new OS name.
- Updated the default sleep screen to display the new custom Seed logo instead of the legacy CrossInk logo.

## [v1.4.0] - 2026-07-10

### Added

- Dashboard UI theme for the Home screen, showing the current book cover and reading stats.
- Nearby Position Sync for sending or applying the current EPUB position between two CrossInk devices over ESP-NOW.
- Web EPUB optimizer support for CrossInk location metadata, so optimized EPUBs can keep better progress and stable page numbers.
- Reading Stats support for XTC and XTCH books, including reader menus, Home and sleep screen stats, mark finished, delete stats, and preserving stats when clearing book caches.
- Web file manager image previews, so PNG, JPEG, BMP, GIF, and WebP files can be viewed inline before downloading.

### Changed

- Large EPUBs, SD-card font-heavy books, and cover thumbnails now open, index, and generate more reliably under low-memory conditions.
- Home and sleep screens now load more cover and thumbnail data only when needed, reducing reader startup work and reusing cached cover data where possible.
- Built-in reader font choices have been reduced to Lexend Deca and Bitter, reducing firmware size while keeping fallback glyph coverage.

### Removed

- Teensy firmware builds are no longer produced for releases or release candidates.

### Fixed

- EPUB render-mode and Safe Mode toast messages now clear reliably, even when the reader is low on memory.
- EPUB Reading Stats no longer drops unsaved page-turn counts after viewing the stats screen mid-session.
- KOSync is more reliable with many SD-card fonts installed, reducing low-memory failures during secure sync requests and uploads.
- Web file manager actions now handle filenames with special characters safely and reject unsafe rename characters before saving.
- Auto Turn interval settings and related action prompts opened from long-press shortcuts now stay open after releasing the shortcut button.
- EPUB footnote previews no longer show clipped status-bar labels or misleading reader progress indicators, and clipping selection now works from footnote previews.
- Font selection no longer reopens the font preview after choosing a font.
- EPUB chapters with stale publisher style data now rebuild it instead of opening without the book's styling.
- Large SD-card font EPUBs no longer overlap characters after font or line-spacing changes, and clipping selection can fall back to a built-in UI font when needed.
- EPUB cover and thumbnail generation is more reliable with custom SD-card fonts selected and optimized books under low-memory conditions.
- Web EPUB optimizer now preserves more PNG and SVG artwork on-device, including transparent PNGs, dividers, and images in malformed or XML-declared chapters.
- Unsupported SVG images in EPUB chapters are now skipped silently instead of triggering low-memory image warnings.
- Nearby Position Sync now silently restarts back into the reader after using ESP-NOW, matching other WiFi sync flows and reducing post-sync memory fragmentation.
- EPUB page cache loading now uses fewer small heap allocations, reducing fragmentation-related reader failures.
- EPUB grayscale page turns on X3 now use the grayscale-aware display base, reducing the moment where new text appears too dark before the anti-aliased overlay finishes.
- EPUB chapters with many inline anchors, footnote links, malformed XHTML, large publisher styles, or SD-card fonts are less likely to fail or get stuck on the indexing screen.
- EPUB opening and image rendering now recover from more low-memory conditions instead of rebooting, including landscape image pages and books that need lighter render modes.
- EPUB clipping selection now follows right-to-left line order when selecting Hebrew and other RTL text.
- Lyra Carousel no longer shows a blank carousel after returning from WiFi-related File Transfer screens and moving between the menu row and book row.
- Generated SD-card font packages now include the same core glyph coverage as built-in reader fonts.
- Manage Fonts no longer crashes while loading or reloading large SD-card font lists.
- Minimal Home no longer swaps to another recent book when returning from Settings when Back button is mapped to the first button.
- Cancelling a font download now stops on the first Cancel button press instead of needing several presses.
- The `Inverted` sleep cover filter now keeps book covers unchanged on Minimal and Dashboard sleep screens while switching the background to white.
- Rare EPUB open or thumbnail crashes during ZIP decompression are fixed.

## [v1.3.4] - 2026-06-24

### Added

- File Browser now indexes large SD-card folders so directories with many books can be browsed without loading every filename into memory at once.
- EPUB text clipping with saved highlights, clipping lists, and Kindle-style `/My Clippings.txt` export.
- `Create Clipping` is now available as a reader shortcut for short/long Power, long-press Menu, and long-press Back actions.
- Per-book EPUB options for font, layout, styling, reading aids, and render modes, including `CrossInk Default`, `Balanced`, and `Light` modes for difficult books.
- Arena allocator (`lib/Memory/Arena.h`) for burst-then-discard allocation patterns - reduces heap fragmentation during EPUB parsing and page layout over long reading sessions.
- Optimized EPUBs now store location metadata at `META-INF/x-locations.json`.
- X3 SD-card writes now use the RTC for file timestamps when the clock is available.

### Changed

- The EPUB reader menu now splits the growing menu into 3 screens, labels per-book settings as `Book Options`, and avoids showing duplicate `Orientation` controls.
- The `Inverted` sleep cover filter now flips Minimal and Reading Stats sleep screens to black text on a white background.

### Fixed

- Calibre Wireless transfer status no longer stacks the last received-file message on top of the upload percentage.
- X3 Tilt Direction now labels left/right choices as `Left-Right` and `Right-Left`, with existing left/right preferences migrated to keep the same physical tilt behavior.
- EPUB layout now honors publisher page-break CSS, avoids stretching justified spaces before closing punctuation, and keeps large CSS rule sets in a smaller disk-backed lookup cache.
- EPUB first-open conversion now uses more compact OPF manifest lookups and streams cover-wrapper parsing to avoid large temporary heap buffers on books with huge manifests.
- EPUB chapters that run out of memory now retry with `Balanced`, `Light`, and final `Safe Mode` rendering before showing an error, apply the same fallbacks during next-chapter pre-indexing, and let book action menus reset a book's reader settings if Safe Mode still cannot open it.
- EPUB reader font-size changes now restore the current chapter position by content instead of jumping far backward after re-indexing.
- Reading Stats now use the reader's last live book time-left estimate instead of showing a separate fallback estimate.
- Per-book reading stats now migrate compatible legacy `stats.bin` files into the `stats_v5.bin` flow instead of resetting when only the old filename exists.
- Lyra Carousel Home menu rendering now avoids extra label allocations that could crash builds under low memory.
- Lyra Carousel Home cover refresh no longer risks a reboot when memory is tight after returning to or selecting a recent book.
- EPUB image-heavy chapters no longer risk a reboot while saving their reading cache under low memory.
- TXT readers now stay open when pressing a page-turn button at the end of the file.
- Long-press reader shortcuts that open another screen no longer close or confirm it again when releasing the shortcut button.
- RoundedRaff's header battery icon and percentage now sit lower to avoid clipping at the top edge.
- Lyra Carousel now keeps the Home header current when rendering the menu or restoring cached carousel frames, preventing stale battery and clock values while navigating between books.
- Web file manager multi-delete now handles larger selections without failing after a small batch.
- Portuguese EPUBs now use Portuguese hyphenation rules instead of leaving long words unhyphenated when Hyphenation is enabled.
- Progressive JPEG EPUB covers now render more smoothly in generated cover and thumbnail BMP assets.
- EPUB section layout now flushes long text runs earlier when Bionic Reading or Guide Dots are enabled, reducing low-memory failures on difficult books.
- Footnotes in EPUBs with very large shared notes sections no longer cause long stalls when opened.
- Firmware updates now follow GitHub asset redirects before streaming the install.
- Tiled grayscale rendering now serializes display transfers on the shared SPI bus to avoid display glitches during SD activity.

## [v1.3.3] - 2026-06-13

### Added

- `File Browser Display` in `Settings > System > Files & Cache` for choosing one-line or two-line file browser rows across all themes, while preserving Minimal users' existing two-line display on upgrade.
- `Hide File Extension` in `Settings > System > Files & Cache` for expanding file-browser filenames by hiding the right-side extension label.
- Device Name in Settings > System > Device for customizing the KOReader Sync and Nearby Stats Sync device label.
- Additional shortcut options and new ability to add custom shortcuts for Long-press Back Action.
- Delete Reading Stats actions in the EPUB reader and book action menus for clearing one book's stats without deleting its cache.

### Changed

- CrossInk settings now save to `/.crosspoint/crossink-settings.json`, with a one-time fallback migration from `/.crosspoint/settings.json`, so switching between firmware builds is less likely to reset preferences.
- The X3 clock visibility setting is now phrased as `Hide Clock`, with existing `Show Clock` preferences migrated to the matching hide behavior.

### Fixed

- RoundedRaff's date shown in settings now sits lower on X3 devices instead of overlapping the battery.
- Clear Bookmark List now asks for confirmation before deleting a book's bookmarks.
- Clear Reading Cache now preserves per-book reading stats while continuing to leave all-time reading stats untouched.
- Moving finished EPUBs to `/Read` now consistently preserves reading progress, per-book stats, bookmarks, and resume state.
- Book settings option lists now return to the submenu they were opened from when pressing Back.
- Lyra Carousel now refreshes its cached Home icon row after OPDS, Reading Stats, or Bookmarks icons appear or disappear.
- KOReader Sync failure screens now wrap long error messages and shut down WiFi cleanly before returning to the book.
- Sleep Screen > Cover now generates the current book cover on demand instead of falling back to the dark sleep screen when the setting is changed after opening a book.
- File Browser now previews PNG images instead of trying to open them as EPUBs, and hides common macOS and Windows metadata files.
- File Browser now refreshes immediately after falling back to the root folder from a stale saved path.
- File Browser now stops loading oversized folders before low memory can crash the device and shows a memory error instead.
- TXT reader long-press Power page turns now work when Long Power Button is set to Page Turn.
- SD-card font read failures no longer risk a reboot while cleaning up the failed file read.
- Page Overlay sleep screens no longer force EPUB chapters to re-index after waking.
- Page Overlay sleep screens now use the current screen as the overlay background outside the reader instead of trying to rebuild a stale book page.

## [v1.3.2] - 2026-06-10

### Added

- Current date in the top-right Settings header on X3 devices.
- Dark Reader Mode for EPUB and TXT reading screens, plus shortcut actions for the power button and front-button long press.
- File Browser long-press folder action for choosing a custom sleep-image folder instead of only `/.sleep` or `/sleep`.
- Expanded X3 Reading Stats, including streaks, time charts, editable dates, all-time backups, reset controls, an idle-time threshold, and the `Minimal Stats` sleep screen.
- `Reset Reading Pace` in the EPUB reader menu when Time Left is enabled, for clearing only the time-left pace estimate while keeping book reading stats.

### Changed

- Display, Reader, and Controls settings now open list menus instead of cycling through options one by one.
- Reading time and time-left pace tracking now ignore page intervals longer than the configured idle-time threshold.
- Web portal pages now use shared templates, stylesheet, and logo assets, reducing on-device page size and improving browser caching.
- Already-cached EPUBs now open directly to the first page without an extra book-loading popup refresh.
- Reader font-size choices now show point sizes like `10 pt` instead of names like `Tiny`.

### Fixed

- Inverted reader menus now honor orientation-aware side-button navigation.
- EPUB book time-left estimates now wait for more session pace samples and use a progress-based floor after pace data exists, reducing swings from unusually short or long pages.
- Deleting an EPUB book cache now preserves that book's reading stats and pace data.
- X3 clock settings now have clearer UTC offset editing, and `Sync Date/Time` can use saved WiFi networks automatically.
- Home, Lyra Carousel, WiFi setup, and SD-card font flows now release memory more aggressively to avoid freezes or crashes on constrained builds.
- Vietnamese settings labels no longer show replacement diamonds after generated translation offsets shifted.
- KOReader Sync now lands correctly at chapter starts and shows more specific connection guidance.
- EPUB bookmarks saved under the old unstable path hash now show up again, including for books moved to `/Read`.
- SD-card font downloads now use versioned direct S3-hosted HTTP endpoints with CRC validation, avoiding GitHub release redirects and ESP32-C3 TLS stalls when loading the font catalog.
- EPUB text blocks now keep the book's alignment style when an inline image appears before the text.

## [v1.3.1] - 2026-05-28

### Added

- EPUB reading-position improvements, including bookmark anchors, bookmark preview snippets, and optional chapter/book time-left estimates.
- Nearby Reading Stats sync with separate totals for this device and all synced CrossInk readers.
- Per-server OPDS filename settings so downloaded books can use either Author - Title or Title - Author.
- EPUB render heap diagnostics that include the largest allocatable block, not just total free heap.

### Changed

- Moved the X3 reader clock into a new top-centered status bar and moved clock settings to Settings > System > Device.
- Reworked Display, Reader, Controls, in-reader options, and larger System settings groups so related options open as submenus.
- Improved OPDS and font download responsiveness by reducing progress-update overhead and temporarily disabling WiFi power saving during transfers.
- Book selection now shows a loading popup before EPUB indexing or cache loading begins.
- Delayed the automatic finished-book prompt until the reader leaves the chapter where they reach 99%.

### Fixed

- WiFi settings screen now keeps the displayed MAC address consistent with the router-visible WiFi address.
- Reader UI issues with inverted menu button hints, Lyra Carousel popups, and Auto Page Turn interval persistence.
- Web uploads and KOReader Sync progress saves now preserve progress, stats, settings, and valid resume data for refreshed book files.
- OPDS low-memory handling now shows a specific parser-buffer memory message and releases SD-card fonts before catalog loading.
- EPUB cache, CSS, table, SD-card font, and allocation failure paths now recover, retry, or stop cleanly under low memory instead of opening unstyled pages, failing unnecessarily, or risking a reboot.
- EPUB text with invisible word-joiner characters no longer shows replacement diamonds for missing font glyphs.
- Clarified the low-memory EPUB image warning so it says some or all images may be missing.

## [v1.3.0] - 2026-05-21

### Added

- Back/Cancel support while downloading books from OPDS catalogs.
- Recent Books long-press menu in both List and Grid views with delete, cache delete, completion, and remove-from-recents actions.
- Minimal sleep screen option that shows the current book cover and reading progress on a dark background.
- More detailed WiFi connection debug logs for scans, selected networks, status changes, disconnect reasons, and timeouts.
- 9pt `Itty Bitty` reader font size, plus build flags for omitting Itty Bitty and Large reader font assets in size-constrained firmware variants.
- In-reader confirmation message when a shortcut turns tilt-to-turn on or off.

### Fixed

- WiFi and OPDS connection-flow edge cases: manual Settings connections now show the connected status before continuing, copied or corrupted saved-password files are rejected before use, OPDS retries show loading before requests, and large OPDS feeds fail safely under low memory instead of rebooting.
- Reader and Home UI polish issues, including landscape status-bar settings, missing Vietnamese labels, File Browser and Lyra Carousel icon alignment, cover thumbnail artifacts, and duplicate Home progress/stat loading.
- EPUB cache and low-memory handling now use stable cache folder keys, migrate older cache folders where possible, rebuild stale section caches, lay out very long text blocks earlier, stream table fallback content when heap is tight, and clarify the warning text.
- Sleep-entry, network, and SD-card font download reliability improvements: cached sleep-screen assets are reused, OPDS pages idle normally after load, the X3 tilt sensor sleeps outside the reader, WiFi power saving is disabled during transfers, WebDAV stack usage is lower, longer stalls are tolerated, interrupted font files are retried, and active reader fonts are freed when needed.
- Remaining reader service edge cases, including an XTC chapter selector crash on memory-constrained builds, SD-card font size selection, SD-card font-size shortcuts skipping manually installed sizes, and KOReader Sync login compatibility with self-hosted servers that return valid JSON on success.

### Changed

- Modified upstream "page-as-sleep" behavior into a new `Sleep Screen > Quick Resume` option, which also keeps `Quick Resume on Timeout` on, and renamed the timeout-only toggle.
- Improved reader and browser menu behavior by moving the Footnotes shortcut above Select Chapter, wrapping long book titles in action menus, and reducing progress-screen repaint work during OPDS and SD font downloads.

## [v1.2.11.1] - 2026-05-15

### Changed

- Removed Medium font size from `xlarge` build to get it below the size limit

### Fixed

- Lyra Carousel is now included by activating the build flag `DCROSSINK_ENABLE_LYRA_CAROUSEL=1`

---

## [v1.2.11] - 2026-05-14

### Added

- New personal theme: "Minimal"
- Custom sleep timer picker so `Time to Sleep` can be set from 1 to 30 minutes instead of cycling fixed presets.
- In-reader Controls shortcut for customizing buttons without leaving the book.
- Bookmark cleanup shortcuts: hold Select on a bookmark to delete it, or hold Open on a book in Bookmarks to clear that book's bookmark list.
- Confirmation message after deleting a book's cache from the reader or File Browser.
- File Browser long-press action for deleting an EPUB or XTC book's cache.
- Downloaded-font size range setting so SD-card fonts can use compact, default, or large point-size sets.
- File Browser long-press action for marking EPUB books as finished or unfinished.

### Changed

- Hardened deep sleep entry by shutting WiFi down before waiting for the power button to be released.
- Raised the web file-transfer filename limit from 100 to 150 bytes so longer uploaded filenames are preserved.
- Made the in-reader Reader Options menu include the same Reader settings and actions as Settings > Reader.
- Split SD-card font descriptions and supported languages into separate lines in the font download screen.

### Fixed

- Inline EPUB images no longer disappear in landscape when their bottom edge slightly overlaps the screen margin.
- Reduced unnecessary low-memory image suppression for JPEG-heavy EPUB chapters and added CSS heap diagnostics during chapter rebuilds.
- Allowed wider inline JPEG images in EPUBs to render when they still fit the total pixel and heap safety limits.
- SD-card font picker no longer reopens immediately after selecting a font from Settings > Reader > Font Family.
- In-reader font-size changes now work for SD-card fonts.
- In-reader SD-card font changes now rebuild the current EPUB page layout consistently.

## [v1.2.10] - 2026-05-11

### Added

- `Recent Books View` setting so the dedicated Recent Books screen can switch between the classic list and a 3x3 cover grid.
- More flexible reader controls, including orientation-aware front/side button settings, nav-only or all-button front inversion, tilt page turn shortcuts, and side-button long-press rotation actions.
- Per-session auto page turn interval picker with values from 5 to 120 seconds.
- File Browser Home/Back long-press action for toggling hidden files and folders.
- EPUB rendering and diagnostics improvements, including visible `<hr>` separators and heap logs around section rebuilds, image extraction, page serialization, and sleep-cache rebuilds.
- Reader font coverage for block redactions, black-square ornaments, Greek category letters, and turned-comma punctuation (PR #104).
- Simulator tools for testing sleep/wake behavior and smoke-testing common screens and EPUB reader menus.

### Changed

- Reduced Controls settings section spacing so the grouped controls fit better on X3 screens.
- Made front reader long-press actions trigger when the hold delay is reached while normal page turns still trigger on release.
- Used the fast EPUB spine/TOC indexing path for books with 300+ spine entries so heavily split books build `book.bin` faster on first open.
- Allowed the web file manager and WebDAV to browse dot-prefixed hidden files when hidden files are enabled, matching the device file browser.

### Fixed

- Reader button and shortcut behavior, including X3 power-button wake filtering, folder delete long-press timing, and WiFi scan/connect screens that could not be exited while work was in progress.
- RoundedRaff home-menu, keyboard, and button-hint rendering issues so Settings remains reachable and compact labels no longer overlap or disappear.
- Font and glyph handling now reduces persistent SD-card font advance-cache memory, releases optional font caches before image extraction only when heap is tight, and shows a visible replacement symbol when compact UI fonts lack `U+FFFD`.
- KOReader Sync authentication diagnostics and an in-reader sync crash, including clearer handling when a server or proxy returns non-JSON content.
- EPUB text rendering for redactions, whitespace-only XHTML text nodes, simple black CSS span backgrounds, list bullets in `<li><p>...</p></li>` items, and very long base64-like text runs.
- EPUB image, thumbnail, and section-rebuild stability so image-heavy chapters use less temporary memory, scale images more reliably, avoid stale dimensions, and suppress optional image work earlier under heap pressure.
- EPUB low-memory and cache safety now skips optional next-chapter indexing and sleep-page cache rebuilds when heap is tight, fails safely with a malformed-book warning and Home exit path, rebuilds incompatible fork-written caches, and handles low-memory CSS parsing, truncated SD writes, invalid serialized strings, and failed temp-cache promotion.
- Home no longer crashes after clearing reading cache when the source EPUB cache is missing.
- Reader prewarm behavior now skips image decoding, keeps mixed-style font glyphs cached together, and avoids section rebuilds for render-quality-only option changes.
- Concurrent render/storage crashes are avoided by serializing `GfxRenderer` scratch-buffer access, shared SPI bus access, and failed SPI lock cleanup.
- Recent Books, EPUB/XTC thumbnail caches, deleted-folder metadata, and XTC cover scaling now keep cached book data in sync and grid covers fill their slots correctly.
- Simulator build configuration now lets SDL2 and simulator-provided network/OTA shims compile cleanly.

---

## [v1.2.9.1] - 2026-05-03

### Changed

- Cleaned up EPUB table rendering by removing synthetic row/cell labels and defaulting table cells to readable left alignment
- Allow simple EPUB tables with full-width note rows so a single `colspan` cell spanning the whole table no longer forces the entire table back to paragraph fallback

### Fixed

- Power-button shortcut conflicts outside the reader so reader-only actions fall back to `Confirm` while Sleep, Refresh, Screenshot, Sync Progress, and File Transfer remain real power actions.
- Potential crash when using `Go to %` in EPUBs.
- Potential crash when entering sleep with Page Overlay enabled if the cached EPUB page data is invalid.
