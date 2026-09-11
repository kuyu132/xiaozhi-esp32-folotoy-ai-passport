#!/usr/bin/env bash
# 烧录 FoloToy AI Passport(仅限用户人工执行!)
# 红线: 严禁 erase_flash; 必须分段写(保护 cardid@0x356000 与 recovery@0x700000)
# 用法: ./scripts/flash-folotoy-passport.sh [/dev/cu.usbmodemXXXX]
set -euo pipefail
cd "$(dirname "$0")/.."
PORT="${1:-$(ls /dev/cu.usbmodem* 2>/dev/null | head -1)}"
[ -z "$PORT" ] && { echo "未找到串口。用法: $0 /dev/cu.usbmodemXXXX"; exit 1; }

echo "=== 以下偏移必须与 build/flasher_args.json 一致(assets 必须为 0x360000)==="
python3 - <<'PYEOF'
import json
d = json.load(open("build/flasher_args.json"))
for off, path in sorted(d["flash_files"].items(), key=lambda kv: int(kv[0], 16)):
    print(f"{off}\t{path}")
PYEOF
read -r -p "确认偏移无误后回车开始烧录 (Ctrl-C 取消): "

python3 -m esptool --chip esp32c3 -p "$PORT" --baud 921600 \
  --before default_reset --after hard_reset write-flash \
  0x0      build/bootloader/bootloader.bin \
  0x8000   build/partition_table/partition-table.bin \
  0x10000  build/xiaozhi.bin \
  0x360000 build/generated_assets.bin

echo "烧录完成。"
