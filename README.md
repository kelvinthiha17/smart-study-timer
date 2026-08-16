# Smart Study Timer

A standalone desk device that tracks study sessions and enforces real breaks, built on an STM32 Nucleo-C031C6. No app, no computer required once it's set up — turn a knob to set your study length, press a button, and the device handles the rest: countdown, break alerts, and break timing, all shown on a 7-segment display with RGB and buzzer feedback.

## Status

- **Breadboard prototype: fully built and tested.** All states, transitions, and hardware (buzzer, RGB, display, potentiometer, button) confirmed working end-to-end.
- **Perfboard build: soldered.** Every component (button, buzzer, transistor, RGB LED, potentiometer, shift register, and display) is permanently mounted and wired.

![Soldered perfboard](photos/soldered-perfboard.jpg)

## How it works

1. **Setting** — turn the potentiometer to select a study duration. The display shows your selection live.
2. **Studying** — press the button to lock in the duration and start the countdown. RGB glows green.
3. **Pause** — press the button anytime during a session to pause. RGB turns amber. Press again to resume exactly where you left off.
4. **Break due** — when the timer hits zero, the RGB blinks red and the buzzer beeps intermittently until acknowledged.
5. **On break** — press the button to start a 15-minute break. RGB blinks blue.
6. **Break over** — when the break ends, the buzzer beeps again until acknowledged, then the device returns to Setting, ready for the next session.

## Hardware

- STM32 Nucleo-C031C6 (Cortex-M0+) — main controller
- 74HC595 shift register — drives the 7-segment display
- 7-segment display (common cathode, 4-digit part, 1 digit wired) — supports a future 2-digit multiplexed upgrade
- RGB LED (common cathode) — state indicator
- Passive buzzer (electromagnetic, 2-pin) — alerts, driven through a transistor
- PN2222 NPN transistor — buzzer driver
- Potentiometer — study duration selector (ADC input)
- Tactile pushbutton — start / pause / acknowledge
- 10× 220Ω resistor — 3 for RGB, 7 for display segments
- 1× 2kΩ resistor — transistor base (buzzer driver)

Full properties and quantities in the [bill of materials](#bill-of-materials) below.

## Pin mapping

| Signal | STM32 Pin | Board Label |
|---|---|---|
| Buzzer driver (transistor base, via 2kΩ) | PB3 | D3 |
| RGB Red (via 220Ω) | PA10 | D2 |
| RGB Green (via 220Ω) | PA15 | D7 |
| RGB Blue (via 220Ω) | PB10 | D4 |
| 74HC595 SER | PB0 | D10 |
| 74HC595 SRCLK | PC7 | D9 |
| 74HC595 RCLK | PA9 | D8 |
| Potentiometer wiper | PA4 | A4 |
| Button | PA0 | A0 |

## Bill of materials

| Ref | Part | Properties |
|---|---|---|
| J1 | STM32 Nucleo board | variant STM32 |
| U1 | 74HC595 shift register | DIP16 [THT] |
| LED1 | RGB LED | common cathode, 5mm THT |
| LED2 | 7-segment display (4-digit) | 1 digit wired |
| Q1 | NPN transistor | TO-92 [THT] |
| R1–R10 | 220Ω resistor ×10 | ±5%, 0.25W |
| R11 | 2kΩ resistor | ±5%, 0.25W |
| R12 | Rotary potentiometer | 10kΩ, linear |
| S1 | Momentary pushbutton | 12mm tactile |
| SG1 | Buzzer | 12mm, 2-pin |

Full bill of materials with all properties: [study_timer_circuit_bom.html](https://htmlpreview.github.io/?https://github.com/kelvinthiha17/smart-study-timer/blob/main/Circuit%20diagram/study_timer_circuit_bom.html)

## Circuit diagram

Full schematic and breadboard-view diagrams were built in [Fritzing](https://fritzing.org/) to document the wiring.

![Fritzing breadboard view](Circuit%20diagram/study_timer_circuit_bb.png)

Editable project file: [`Circuit diagram/study_timer_circuit.fzz`](Circuit%20diagram/study_timer_circuit.fzz)

## Software design notes

- **State machine**: `SETTING → STUDYING ⇄ PAUSED → BREAK_DUE → ON_BREAK ⇄ PAUSED → BREAK_OVER → SETTING`, implemented with a single `system_state` enum and driven by a 1Hz hardware timer (TIM3) plus a polled button.
- **Button input is polled, not interrupt-driven.** The buzzer's electromagnetic coil injected voltage spikes that were falsely triggering the button's EXTI interrupt. Switching to a debounced poll (requiring several consecutive low readings before registering a press) fixed it without adding a flyback diode.
- **Potentiometer reading** uses 32-sample averaging plus self-calibrating min/max tracking, since the pot's physical rotation doesn't reach the ADC's full 0–4095 range. The mapped value snaps to fixed steps (10/20/30/40/50/60 minutes) for reliable, repeatable selection.
- **Display** shows two-digit values by sequentially flashing tens → blank → ones on the single wired digit, only refreshing when the value actually changes (not continuously), to keep it readable and non-distracting. Single-digit values (0–9) display steadily with no flash needed.

## What's next

- Add a second transistor to enable true simultaneous 2-digit (or full 4-digit) display via multiplexing, using the display's unused digit positions.
- Add a flyback diode across the buzzer for cleaner switching (currently handled in software via button debouncing instead).

## Repo structure

```
├── Core/Src/main.c                          # Firmware
├── Circuit diagram/
│   ├── study_timer_circuit.fzz              # Editable Fritzing project
│   ├── study_timer_circuit_bb.png           # Breadboard-view diagram
│   └── study_timer_circuit_bom.html         # Full bill of materials
├── photos/
│   └── soldered-perfboard.jpg
└── README.md
```
