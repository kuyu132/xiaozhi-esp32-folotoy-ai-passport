# Changelog

## 2.6.0-folotoy.1 - 2026-11-19

### Features

- Redesign the AI Passport 240×320 assistant screen with a compact expression avatar,
  separate question/answer cards, thinking feedback and state-specific key hints.
- Keep the most recent answer readable after returning to idle; support theme,
  font and subtitle-visibility changes.

### Fixes

- Keep reply text wrapped and scroll the entire reply viewport upward when it
  overflows, instead of turning the first line into a horizontal marquee.
- Remove duplicate wake-word prompts from the standby screen.
- Detect the GDMA API before enabling the compatibility shim, restoring clean
  builds with ESP-IDF v6.1 final.
- Use numeric application version `2.6.0` so OTA version parsing succeeds.
