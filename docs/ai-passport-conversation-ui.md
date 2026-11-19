# AI Passport conversation UI

The `ai-passport` and `ai-passport-ww` firmware variants use a dedicated 240×320
portrait conversation display. Network, status and battery have separate space
in the top row. A compact expression avatar indicates listening with a green
outline and speaking with a blue outline. The latest question appears as a
single-line summary; the response occupies the remaining body area.

Reply labels stay in WRAP mode at a fixed width and natural text height.
Only their parent viewport scrolls, strictly on the Y axis. Short replies
remain still. Overflowing paragraphs move upward after a two-second pause,
hold briefly at the bottom, then repeat from the top. A newly received sentence
replaces the prior sentence and restarts its scroll; this is not a full-history
chat transcript. Returning to idle retains the latest answer until a new
question or system message replaces it.

The centre standby prompt explains voice wake-up (or the OK key in the standard
variant). The footer only shows key actions, avoiding duplicate prompts. Theme
and font changes recalculate wrapping; hiding subtitles hides both text cards.
The board's existing long-press quotation-reader shortcut remains available.

## Validation

Both variants must build with ESP-IDF v6.1 and fit the protected 8MB partition
layout. On-device testing on 2026-11-19 confirmed display/audio initialization,
Wi-Fi/MQTT connectivity, offline wake-up and speech replies. A long-paragraph
test produced a 190×122 viewport and 171-pixel wrapped text height, with 49 pixels
of vertical overflow; scroll logs showed X=0 and increasing Y. Short messages
had zero overflow and no scrolling. These checks establish on-device layout
geometry and movement, not visual verification of every theme or expression.

Release ZIPs contain only the bootloader, partition table, application, assets,
OTA initialization data and flash parameters. They must not include NVS,
cardid, recovery images, full-flash backups or merged binary images. Use the
segmented addresses in `flash_args` and preserve device configuration.
