# Hi there, I'm Cameron (0xKnowles) 👋

I am a Software and Hardware Developer specializing in full-stack applications, custom embedded firmware, and physical hardware telemetry. I build solutions that bridge the gap between elegant user interfaces and physical device engineering.

- 🚀 **Core Stack:** TypeScript, React, Angular, Node.js, C/C++, and KiCad for hardware design.
- 🛠️ **Current Focus:** Custom embedded firmware optimizations, e-ink interfaces, and automated cultivation monitors.
- 🌱 **Domain Expertise:** A strong background in IT infrastructure paired with deep experience in Integrated Pest Management (IPM) and greenhouse automation.

---

### 📂 Featured Projects

#### 🌿🐣 [CrossPlant XTEink](https://github.com/0xKnowles/CrossPlant-Xteink)
An open-source custom firmware fork for the **XTEink X3 and X4 mini e-ink readers**. It merges the advanced typography and performance optimizations of *CrossInk* with a re-engineered virtual plant companion mechanics system (*CrossPet*).

* **The Virtual Plant System:** Features a reading-driven ecosystem where plant stats (Moisture, Sunlight, Health, Nutrients) decay in real time while awake, evolving through branches based on actual reading habits, streaks, and local environmental factors.
* **Premium Features:** 
  * A space-efficient **Two-Column UI** separating plant vitals from detailed reading stats.
  * **Water & Fertilizer Stock System:** Integrated Water Jug (3 charges) and Fertilizer (3 charges) stock mechanics. Water is refilled for free, and fertilizer can be purchased for 30 DD directly from the main action list.
  * **Dew Shop & Passive Upgrades:** Replaced cosmetic accessories with functional tools that grant passive vitals boosts, such as the Moss Pole (halves sunlight decay), Self-Watering Pot (halves moisture decay), Greenhouse Cover (halves health decay), and Slow-Release Fertilizer (auto-regenerates nutrients).
  * **Real-time Weather Sync:** Periodically geolocates the device using the wireless IP and queries Open-Meteo to apply passive hourly growth boosts based on local conditions (Sunny, Rainy, Cloudy, Snowy), visible on both the main UI card and the sleep screen.
  * **Herbarium Catalog:** A built-in discovery log showing discovery percentages (e.g., 4/12) and unlocking progress for the 4 growth stages of the 3 plant types (Alocasia, Begonia, Monstera).
  * **Dynamic Reading-Based Quests:** Replaced standard tasks with reading-focused achievements (Speedy Reader, Night Owl, Streak Saver) rendered in a clean, non-overlapping stacked UI.
  * **Plant Diary Sleep Screen:** A dual-pane standby screen displaying your sleeping plant alongside an RTC-synced daily achievement summary and local weather bonus telemetry.
  * **Hardware Optimizations:** High-fidelity SD card `.bmp` asset probing, custom bezel key long-press shortcuts, and a research-backed typography engine featuring *Lexend Deca* and *Bitter*.

#### 🎣 [Bio Tackle Box](https://github.com/0xKnowles/Bio-Tackle-Box)
A dedicated, full-stack application designed for agricultural management. It streamlines digital tracking, workflows, and essential data management for specialized growing environments.
* **Stack:** TypeScript, React, and Angular framework infrastructure.

#### 💧 HydroLogix / DWC Monitor (Custom Hardware)
An automated Deep Water Culture (DWC) environmental monitor built from the ground up to track real-time reservoir metrics.
* **Hardware & Firmware:** Custom schematic capture and PCB layout routed entirely in **KiCad**.
* **Features:** Utilizes an Elegoo HC-SR04 ultrasonic sensor for precise, real-time liquid-level tracking and environmental telemetry, all housed in a custom-designed, 3D-printed enclosure.

---

### 💻 Technical Toolbelt

<table>
  <tr>
    <td align="center" width="100">
      <img src="https://skillicons.dev/icons?i=cpp" width="48" height="48" alt="C++" />
      <br>C / C++
    </td>
    <td align="center" width="100">
      <img src="https://skillicons.dev/icons?i=kicad" width="48" height="48" alt="KiCad" />
      <br>KiCad
    </td>
    <td align="center" width="100">
      <img src="https://skillicons.dev/icons?i=react" width="48" height="48" alt="React" />
      <br>React
    </td>
    <td align="center" width="100">
      <img src="https://skillicons.dev/icons?i=angular" width="48" height="48" alt="Angular" />
      <br>Angular
    </td>
    <td align="center" width="100">
      <img src="https://skillicons.dev/icons?i=ts" width="48" height="48" alt="TypeScript" />
      <br>TypeScript
    </td>
    <td align="center" width="100">
      <img src="https://skillicons.dev/icons?i=nodejs" width="48" height="48" alt="Node.js" />
      <br>Node.js
    </td>
  </tr>
</table>

### 🛠️ Hardware & Fabrication Capabilities
* **PCB Routing & Embedded Design:** Multilayer board layouts, physical input re-mapping, low-power display optimization, and hardware interrupt handling.
* **Rapid Prototyping:** 3D printing custom mechanical enclosures tailored to exact circuit and button configurations.
* **Telemetry & I/O:** Interfacing custom edge sensor arrays with digital frontend dashboards and embedded UI layouts.