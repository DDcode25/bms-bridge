#!/usr/bin/env bash
# Хостовые unit-тесты обычным gcc, без сборки ESP-IDF и без libbsd-dev.
# Unity берётся из ESP-IDF ($IDF_PATH). Запуск: test/host_test.sh
set -euo pipefail

here="$(cd "$(dirname "$0")" && pwd)"
root="$here/.."
idf="${IDF_PATH:?нужен IDF_PATH (source export.sh / activate_idf_*.sh)}"
out="$here/build_host"
mkdir -p "$out"

# -B/usr/bin: после активации ESP-IDF в PATH раньше системного стоит ld из esp32ulp.
cc="${CC:-gcc}"
"$cc" -B/usr/bin -std=gnu11 -O1 -g -Wall -Wextra -Werror \
    -fsanitize=address,undefined -fno-omit-frame-pointer \
    -I"$idf/components/unity/unity/src" \
    -I"$root/components/pack_model/include" \
    -I"$root/components/limits/include" \
    -I"$root/components/pylon_rs485/include" \
    -I"$root/components/bmz_driver/include" \
    -I"$here/main" \
    "$idf/components/unity/unity/src/unity.c" \
    "$root/components/pack_model/pack_aggregate.c" \
    "$root/components/limits/limits_calc.c" \
    "$root/components/pylon_rs485/pylon_frame.c" \
    "$root/components/pylon_rs485/pylon_cmd.c" \
    "$root/components/bmz_driver/bmz_can.c" \
    "$here"/main/*.c \
    -o "$out/tests"

"$out/tests"
