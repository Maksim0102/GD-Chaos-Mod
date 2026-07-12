# Chaos Mod for Geometry Dash

**Chaos Mod** is an exciting Geode mod that turns normal level completion into a wild challenge! Every 10 seconds, a random chaotic event activates, completely changing the gameplay rules.

---

## Available Events (Effects):

* **Speedup:** Increases the game speed to **1.5x**. Your reflexes will be tested to the limit!
* **Slowdown:** Decreases the game speed to **0.75x**, creating a sluggish atmosphere where timings are radically altered.
* **Mirror:** Horizontally mirrors the screen and the level.
  * *Unique Feature:* Implements smart XOR portal logic. Passing through a native mirror portal during the event temporarily reverts the flip, and leaving it brings the mirror back.
* **Old:** The game gets rendered in a vintage black-and-white color palette with authentic CRT scanline overlays.
* **Flashbang:** Blinds the player with powerful white flashes three times during the event, completely blocking the screen for half a second before fading out.
* **Noclip:** A temporary lifesaver! The player becomes a ghost, blinking visually and passing safely through any walls and spikes.
* **Spin:** Smoothly rotates the screen **1080 degrees** over 9.9 seconds using Geometry Dash 2.2's native camera Rotate trigger mechanism.
* **Invisible:** The player becomes completely invisible, and all level objects fade out to an extreme **5% opacity**.

---

## Polished User Interface (HUD):

A neat, high-contrast, and vibrant HUD panel is located in the top-left corner of the screen:
* **Event Name:** Features colorful animated text that pops up dynamically with every event transition.
* **Countdown Timer:** Shows the remaining time of the active event using a bright neon cyan font.
* **Survival Counter:** Tracks the number of successfully survived events in a bright gold-yellow neon font.
* The HUD panel is **100% solid/opaque**, ensuring perfect readability on any background without clashing with Geometry Dash's progress bar.

---

## Mod Settings:

In the Geode settings menu, you can fully customize your chaotic experience:
* **Toggle Individual Events:** Easily enable or disable any event in the pool using checkboxes.
* **Normal Can Repeat:** Configure whether the safe "NORMAL" interval can appear during gameplay, or only at the level start.
