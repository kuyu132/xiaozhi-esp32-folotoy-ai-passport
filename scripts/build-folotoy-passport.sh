#!/usr/bin/env bash
# 构建 FoloToy AI Passport 固件(在 Docker 的 ESP-IDF 6.1 环境中)
# 用法: ./scripts/build-folotoy-passport.sh [ai-passport|ai-passport-ww]
set -euo pipefail
cd "$(dirname "$0")/.."
VARIANT="${1:-ai-passport}"
IDF_IMAGE="espressif/idf:release-v6.1"

docker run --rm \
  -v "$PWD":/workspace \
  -w /workspace \
  "$IDF_IMAGE" \
  bash -c 'python scripts/build.py folotoy/ai-passport --name "$0"' "$VARIANT"

echo "构建完成, 变体: $VARIANT"
echo "产物(烧录请用 scripts/flash-folotoy-passport.sh, 严禁整片写 merged-binary.bin):"
ls -l build/bootloader/bootloader.bin build/partition_table/partition-table.bin \
      build/xiaozhi.bin build/generated_assets.bin
