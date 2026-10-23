# NEW

A new firmware for the M-VAVE FM-1, on [fm1-base](https://github.com/charlesvestal/fm1-base) (in `base/`, a git
submodule). It starts as fm1-base's template, the smallest complete fm1-base firmware: "NEW" is a working name.

## Build and test

    ./build.sh --host                                               # the host simulator, then its checks:
    app/build/host/new_host app/tests/basic.txt app/build/host
    app/build/host/new_host app/tests/screens.txt app/docs/img      # the manual's screenshots
    ./build.sh --web                                                # the browser emulator: app/build/emu/
    python3 -m http.server -d app/build/emu 8770                    # then http://localhost:8770
    ./build.sh                                                      # the firmware: app/build/new.fwsc

`build.sh` finds its tools on its own: Python with Pillow in `~/.venvs/fm1`, Emscripten in `~/.fm1/emsdk`, the
JieLi toolchain in `~/.jieli/toolchain` and the AC79 SDK in `~/fw-AC79_AIoT_SDK` (each can be set: `PYTHON`,
`EMSDK`, `JIELI_TOOLCHAIN`, `AC79_SDK`).

After a fresh clone: `git submodule update --init --recursive`.

## Giving it its own name

| File | What to set |
| --- | --- |
| `app/app.h` | `APP_NAME`, `APP_SPLASH_RGB` |
| `app/fm1.toml` | `name`, `usb_manufacturer`, `usb_product`, `tagline`, `repo` (`id_prefix` FM-1_48 is this firmware's) |
| `app/words.tsv` | the `# url:` line, and the name's line |
| `app/tests/*.txt` | the host binary's name in the comments, `said NEW` in basic.txt |
| `app/docs/MANUAL.md`, `app/site/landing.md` | the template's example text, to rewrite |

## Layout

| Path | What |
| --- | --- |
| `app/app.c`, `app/app.h` | the app: input, sound, screen, saving; its half of the contract with the platform |
| `app/fm1.toml` | build and site settings |
| `app/words.tsv`, `app/web/words/` | the screen reader's words; every words file ever made is kept |
| `app/tests/` | the host simulator's checks |
| `app/docs/`, `app/site/` | the manual and the site's front page |
| `base/` | fm1-base (MPL-2.0), with felucca-bsp in `base/bsp/` |

fm1-base is MPL-2.0: changes to its files are published under MPL-2.0.
