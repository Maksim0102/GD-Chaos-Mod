# Chaos Mod for Geometry Dash 2.2

<p align="center">
  <img src="logo.png" width="180" alt="Chaos Mod Logo" />
</p>

**Chaos Mod** is a highly dynamic and feature-rich Geode plugin for Geometry Dash 2.2. It introduces a thrilling gameplay twist: **every 10 seconds, a random chaotic event triggers, radically turning your levels upside down!** 

From sudden speed shifts and horizontally mirrored perspectives to blinding flashes, screen spins, and complete invisibilities — this mod will push your reflexes, patience, and GD mastery to their absolute limits.

---

## 🌪️ Active Events (Effects Pool)

All events are fully customizable and can be individually toggled in the Geode settings:

* **🏎️ Speedup:** Increases the game speed to **1.5x**. Your quick decision-making is tested to the extreme!
* **🐢 Slowdown:** Decreases the game speed to **0.75x**, shifting all level timings into a sluggish, heavy rhythm.
* **🪞 Mirror:** Horizontally mirrors your screen and level geometry.
  * *Smart XOR-Logic:* If you pass a native mirror portal in the level while the event is active, the mirroring temporarily reverts to normal, and then flips back once the event ends or when you exit the portal!
* **📺 Old:** Implements an aesthetic greyscale black-and-white shader overlay combined with moving analog CRT horizontal scanlines.
* **💥 Flashbang:** Triggers three intense, blinding white flashes throughout the 10-second window, completely covering your screen for half a second before fading.
* **👻 Noclip:** A temporary savior! The player blinks visually, becomes a semi-transparent ghost, and receives **complete invulnerability** to fly through spikes and walls safely.
* **🌀 Spin:** Smoothly rotates your screen **1080 degrees** over 9.9 seconds using Geometry Dash 2.2's native camera rotation trigger engine.
* **🌫️ Invisible:** Your player icon disappears completely, and all level blocks, background details, and obstacles fade out to an extreme, challenging **5% opacity**.

---

## 🎨 Clean & Solid HUD Interface

The mod features a beautiful, non-transparent, high-contrast HUD panel carefully positioned in the **top-left corner**:
* **Event Label:** Displays the currently active effect using high-visibility vibrant text colors tailored for each event.
* **Vibrant Neon Timers:** The countdown timer glows with a neon cyan-blue font (`0, 240, 255`), and the survival counter shines in bright gold-yellow (`255, 220, 0`).
* **Solid Background:** Unlike standard semi-transparent HUDs, this background is **100% solid and opaque**, ensuring flawless legibility against any background and preventing any overlapping confusion with Geometry Dash's progress bar.

---

## ⚙️ Configuration & Customization

The mod is built with complete user freedom in mind. Accessing Geode's settings menu allows you to:
* **Toggle Individual Events:** Turn checkboxes on or off to create your own customized pool of chaos.
* **First Attempt Delay (Intro Delay):** Built-in support for a 1-second delay during the level's very first try, allowing you to prepare before the timer starts. Subsequent attempts start instantly!
* **Normal Event Repeat:** Configure whether the safe, classic "NORMAL" gameplay interval can reappear randomly in the event cycle, or only on level start.

---

## 🔨 Development & Build Instructions

If you would like to compile the source code yourself:

### Prerequisites
1. Installed **Geode SDK** and **Geode CLI** (refer to [Geode SDK Docs](https://docs.geode-sdk.org/getting-started/)).
2. C++ Compiler supporting C++20 (e.g., MSVC on Windows).

### Compiling
Run the standard Geode build command in the root folder of the project:
```sh
# Compiles the source and builds the .geode package
geode build
```
Once built, the packaged `.geode` file will be generated in your build directory, ready to be copied into your Geometry Dash `mods` directory!

---

## 📜 Credits & Resources
* Built using the powerful [Geode SDK](https://github.com/geode-sdk/geode).
* Inspired by classic gaming Chaos Mods.
