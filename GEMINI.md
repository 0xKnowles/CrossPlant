# PocketPet Architecture & Rules

## Project Context
- **Base Firmware:** CrossInk (The newest release/fork of CrossPoint for the XTEink X3 e-ink hardware).
- **Injected Feature:** "My Pet" feature pulled from a community fork called "CrossPet".
- **Current Status:** Claude successfully injected the 'My Pet' logic into the CrossInk base. It builds perfectly via GitHub Actions CI and outputs a working `.bin` firmware that flashes and runs on the X3.

## Technical Environment
- **Target Hardware:** XTEink X3 (E-ink screen constraints apply—low refresh rate, optimized UI rendering).
- **Output:** Firmware compilation outputs a flashable `.bin` file.

## Developer Preferences & Constraints
- Keep e-ink display limitations in mind (avoid heavy full-screen flashing animation loops; favor partial screen refreshes where possible).
- Before writing structural code changes, always analyze how the 'My Pet' loop interacts with the core CrossInk event loop.
