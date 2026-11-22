#!/bin/sh
# Builds the host harness and renders the README previews into docs/.
# Needs gcc and Python with Pillow. Run from the repository root.
set -e
cd "$(dirname "$0")"
gcc -std=gnu99 -Wall -Wextra -Werror -Wno-unused-parameter -I. -o stub_rect stub.c ../../src/c/draw.c ../../src/c/text.c ../../src/c/weather.c ../../src/c/icons.c -lm
gcc -std=gnu99 -Wall -Wextra -Werror -Wno-unused-parameter -DPBL_ROUND -I. -o stub_round stub.c ../../src/c/draw.c ../../src/c/text.c ../../src/c/weather.c ../../src/c/icons.c -lm
# args: theme lang scenario range corner scrub hhmm tilt
./stub_round 260 260 0 0 0 24 4 -1 0800 0  | python3 render.py 260 260 1 ../../docs/round-midnight.png
./stub_round 260 260 1 1 1 24 2 -1 2030 0  | python3 render.py 260 260 1 ../../docs/round-daylight.png
./stub_round 260 260 3 0 2 24 1 -1 1407 30 | python3 render.py 260 260 1 ../../docs/round-tilted.png
./stub_rect 200 228 0 0 0 24 4 -1 1407 0   | python3 render.py 200 228 0 ../../docs/time2-midnight.png
./stub_rect 200 228 0 0 0 24 1 75 1407 0   | python3 render.py 200 228 0 ../../docs/time2-flick.png
./stub_round 260 260 0 0 0 24 4 -1 1046 0 F9 EE | python3 render.py 260 260 1 ../../docs/round-own-colours.png
