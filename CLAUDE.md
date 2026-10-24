# Cardputer Battleship: handover

## What this is
A two-player Battleship game played directly between two M5Stack Cardputer ADV devices, with no router and no phone. It's a personal weekend project for Lars Emil and his girlfriend. It is standalone firmware and has nothing to do with the In The Loop app loader.

The look, game flow and sounds were designed in a browser mock: `reference/demo.html`. Open it in a browser, because it is the spec. Two devices sit side by side, a bot can play the second one, and the middle column logs every radio message. The drawing code in it (canvas JavaScript) is meant to be ported fairly directly to the device.

## How to work with Lars Emil
- He is very experienced with automation, web and n8n, but a **complete beginner in embedded C++ and microcontroller tooling** (PlatformIO, flashing, libraries, etc.). Explain jargon in plain language, say what each step is for, and don't assume background there.
- He tests every step on the real devices. Work in small steps that each end in something he can flash and see.
- He is on a weekend trip with a laptop. Keep setup simple.
- Use git from the start and commit after each working milestone.

## Hardware (verified on the loader project)
- Cardputer ADV: StampS3A (ESP32-S3FN8, 8 MB flash, **no PSRAM**), ST7789V2 display at 240x135, TCA8418 keyboard controller (4x14 key matrix), built-in speaker, battery. There is a microSD slot, but don't rely on it.
- Key mapping verified on hardware: the arrow keys report `;` `.` `,` `/` (up, down, left, right). `esc` reports backtick (96). `ok`/enter and `del` come through as named flags.
- The web flasher needed the device switched OFF to connect. For USB uploads from PlatformIO, if the upload can't find the device, put it in download mode by holding the G0 button while plugging in USB.

## Toolchain (suggested; verify current versions)
- VS Code + PlatformIO, Arduino framework.
- M5Unified / M5GFX for display, speaker and buttons, plus the M5Cardputer library for the keyboard. **Check that the library version supports the ADV's TCA8418 keyboard**, because older Cardputer code assumed a GPIO key matrix.
- First job: get the toolchain working and a hello-world on both devices before writing game code.

## Game rules (as in the mock)
- 10x10 grid, columns A-J, rows 1-10. Five ships: Carrier 5, Battleship 4, Cruiser 3, Submarine 3, Destroyer 2.
- Placement: move with the arrow keys. `r` rotates, `ok` places, `a` auto-places the remaining ships. A ghost ship shows green when it fits and red when it doesn't.
- Whoever finishes placing first fires first. Turns alternate strictly: a hit does **not** grant another shot (decided by Lars).
- Two views: "your shots" and "your fleet". `tab` switches between them. When the partner fires, the screen jumps to your fleet for about 1.3 s to show the impact, then back to your shots on your turn.
- After a game, both press `ok` for a rematch. The win/loss score persists across games and should survive power off (store it in NVS, the chip's small built-in settings storage).
- Suggested extra key: `m` toggles sound.

## Radio: ESP-NOW
ESP-NOW is Espressif's direct chip-to-chip radio link. It needs no router and no pairing, and both devices run identical firmware.
- **Discovery:** broadcast HELLO on a fixed WiFi channel (for example 1). When a HELLO arrives, lock onto that sender's MAC address and ignore other devices. Other Cardputers from the conference may be nearby.
- **Messages,** matching the mock's log:
  - `HELLO {name}`
  - `READY`
  - `FIRE {x,y}`
  - `RESULT {x,y,hit, sunk?: shipName + cells, win?}`
  - `REMATCH`
- ESP-NOW can drop packets. Give each message a sequence number, acknowledge it, and retry until acknowledged. Ignore duplicates.
- **Heartbeat:** send one every second or so. If the partner goes silent for a few seconds, show "partner lost" and go back to searching. A full mid-game resync is a nice-to-have, not required.

## Screen layout (240x135)
- **Grid:** at x=16, y=12, 11 px cells. Letters above, numbers to the left. The column and row under the cursor are highlighted.
- **Side panel:** at x=134, about 104 px wide, rounded, with a dark gradient.
- **Text sizes:** **8 px tall is the minimum** for any text. Headings ("YOUR TURN", "PLACE FLEET") are 10-11 px bold. The coordinate being aimed at (like B7) is about 20 px. Smooth fonts via M5GFX are welcome.
- **Battle panel:** turn banner (green and pulsing on your turn), current view plus a `tab` hint, big target coordinate, a two-line message, small ship icons for ships left (them / you), score, and an `ok fire` hint.

## Graphics
Draw each frame into one full-screen off-screen buffer (an M5Canvas sprite, 240x135 at 16-bit, about 65 KB) and push it to the display in one go, so nothing flickers. Aim for about 30 fps. All of the following is in the mock:
- **Sea:** animated with small moving wave strokes. A darker "fog" sea on the shots view.
- **Ships:** hulls with a pointed bow, guns and a bridge. Sunk ships turn dark red.
- **Cursor:** pulsing corner brackets.
- **Firing:** a target ring that shrinks onto the square.
- **Hit:** an explosion flash with sparks, a short screen shake, and the square keeps burning with flickering flames.
- **Miss:** a splash with ripples and droplets, leaving a white ring marker.
- **Sunk:** a "SUNK!" banner slides across the board, and smoke rises from the wreck.
- **Other screens:**
  - Search screen: a radar sweep with blips.
  - Win screen: fireworks.
  - Loss screen: red tint with "FLEET SUNK".

## Sound
Use the speaker through M5.Speaker. Start with synthesized tones. Short WAV samples for the boom and splash are an optional upgrade later. Events to cover:
- place
- invalid move
- fire
- hit (rumble)
- miss (splash)
- sunk (hit plus a descending tune)
- win fanfare
- "your turn" chime
- link found

## Suggested milestones
Flash both devices and test after each one.
1. Toolchain plus hello-world on both devices.
2. Keyboard and screen: draw the grid and move the cursor.
3. ESP-NOW link test: the two devices find each other and show a ping counter.
4. Placement phase.
5. Battle, the full protocol with acks, win screen and rematch.
6. Polish: graphics, sound, score saved in NVS.
