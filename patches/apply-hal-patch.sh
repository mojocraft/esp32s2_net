#!/bin/bash
# 应用 hal_espressif 的 ESP32-S2 WiFi+PSRAM 补丁（幂等）。
#
# 三态逻辑：
#   - 未打过   -> 应用补丁
#   - 已打过   -> 跳过
#   - 有冲突   -> 报错退出（通常是 west update 升级了 hal，对照 README 处理）
#
# 用法:
#   bash patches/apply-hal-patch.sh
#
# hal_espressif 路径可用环境变量 HAL_DIR 覆盖：
#   HAL_DIR=/其他路径/modules/hal/espressif bash patches/apply-hal-patch.sh

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
HAL_DIR="${HAL_DIR:-/home/mojo/zephyrproject/modules/hal/espressif}"
PATCH="$SCRIPT_DIR/hal_espressif-0001-esp32s2-wifi-psram-internal-alloc.patch"

if [ ! -d "$HAL_DIR/.git" ]; then
    echo "错误: 找不到 hal_espressif 仓库: $HAL_DIR" >&2
    echo "      可用 HAL_DIR=<路径> 覆盖" >&2
    exit 1
fi

if [ ! -f "$PATCH" ]; then
    echo "错误: 找不到补丁文件: $PATCH" >&2
    exit 1
fi

if git -C "$HAL_DIR" apply --check "$PATCH" 2>/dev/null; then
    git -C "$HAL_DIR" apply "$PATCH"
    echo "✅ 补丁应用成功: $HAL_DIR"
elif git -C "$HAL_DIR" apply --check --reverse "$PATCH" 2>/dev/null; then
    echo "✅ 补丁已应用，跳过"
else
    echo "❌ 补丁与当前 hal 源码冲突，无法应用（可能 hal_espressif 已升级）" >&2
    echo "   请对照 README.md「补丁失效（冲突）时怎么办」一节处理" >&2
    exit 1
fi
