#!/bin/sh
# SPDX-License-Identifier: MPL-2.0
# Build FM-1_DelRio with fm1-base (base/, a git submodule).
#   ./build.sh [--release X.Y] [--host | --web]
#   ./build.sh --site [X.Y]      the release X.Y (default 0.1), the emulator and the site beside them in app/build/site:
#                                its emu/ has "Install on the FM-1", which opens its install/ (Chrome or Edge: Web MIDI)
#   JIELI_TOOLCHAIN  JieLi Linux toolchain (default: ~/.jieli/toolchain)
#   AC79_SDK         JieLi AC79 SDK checkout (default: ~/fw-AC79_AIoT_SDK)
#   PYTHON           a Python with Pillow (default: ~/.venvs/fm1/bin/python if it exists, else python3)
#   EMSDK            Emscripten, for --web when emcc isn't on the PATH (default: ~/.fm1/emsdk)
set -e
cd "$(dirname "$0")"
export JIELI_TOOLCHAIN="${JIELI_TOOLCHAIN:-$HOME/.jieli/toolchain}"
export AC79_SDK="${AC79_SDK:-$HOME/fw-AC79_AIoT_SDK}"
if [ -z "$PYTHON" ] && [ -x "$HOME/.venvs/fm1/bin/python" ]; then PYTHON="$HOME/.venvs/fm1/bin/python"; fi
PY="${PYTHON:-python3}"
case " $* " in *" --web "*)
	if ! command -v emcc >/dev/null 2>&1; then
		EMSDK_DIR="${EMSDK:-$HOME/.fm1/emsdk}"
		[ -f "$EMSDK_DIR/emsdk_env.sh" ] || { echo "build.sh: no emcc on the PATH and no Emscripten in $EMSDK_DIR (set EMSDK)"; exit 1; }
		. "$EMSDK_DIR/emsdk_env.sh" >/dev/null 2>&1
	fi;;
esac
[ -f base/tools/build.py ] || { echo "build.sh: no fm1-base in base/ (git submodule update --init --recursive)"; exit 1; }
"$PY" -c 'import PIL' 2>/dev/null || { echo "build.sh: $PY has no Pillow (python3 -m venv ~/.venvs/fm1 && ~/.venvs/fm1/bin/pip install Pillow numpy)"; exit 1; }
if [ "$1" = "--site" ]; then
	V="${2:-0.1}"
	"$0" --release "$V"
	"$0" --web
	exec "$PY" base/tools/make_site.py app "app/build/delrio-$V.fwsc" "$V" app/build/site   # fm1.toml's name, lower case
fi
exec "$PY" base/tools/build.py app "$@"
