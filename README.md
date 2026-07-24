# tuyaos_demo_wukong_ai (RK3506B port)

App **source + vendor platform both live here** (`atk/app/tuyaos_demo_wukong_ai/`).
It builds **self-contained** for ATK-DLRK3506B (32-bit armhf) — no external
TuyaOS tree is needed. SDK headers (`include/`), libs (`libs/libtuyaos.a`),
the armhf toolchain (`vendor/.../toolchain`) and the TKL adapter source
(`vendor/.../tuyaos_adapter/src`) are all in this directory.

`apps/` is **flat** — app source sits directly under `apps/` (not under
`apps/<APP_NAME>/`). The build system is patched to match:
`scripts/Makefile`, `scripts/mk/app.mk`, `apps/local.mk`, `build/build_param`
and `vendor/.../tuyaos/Makefile` resolve every path to this repo (the former
`apps/$(APP_NAME)` becomes `apps`, and the old external TuyaOS-tree roots now
point at this app dir). Each patched file keeps an
`.orig` backup.

## Layout

```
app/tuyaos_demo_wukong_ai/
  apps/                                         app source (flat): src/, build/, include/, local.mk, CMakeLists.txt
    src/boards/RK3506B_BOARD/                   RK3506B board
  include/, libs/                               SDK headers + libtuyaos.a
  vendor/gcc-arm-10.3-…-gnueabihf/
    toolchain/                                  armhf gcc (must be fully extracted — cc1 under libexec/gcc/.../)
    tuyaos/tuyaos_adapter/src/tkl_*.c           TKL adapter (WiFi/BT/flash/stubs)
    tuyaos/Makefile                             PATCHED: TUYAOS_ROOT/CXX -> this repo
  build/, scripts/                              build config + xmake framework
  build_rk3506b.sh                              build entry (run from here)
  apps/output/tuyaos_demo_wukong_ai_1.0.55/     produced binary
```

## Build

```
cd app/tuyaos_demo_wukong_ai
./build_rk3506b.sh            # -> apps/output/tuyaos_demo_wukong_ai_1.0.55/tuyaos_demo_wukong_ai
```

`build_rk3506b.sh` runs `make -f scripts/Makefile app_by_name` in this dir
(compiles `apps/src` + the vendor adapter, then links via
`vendor/.../tuyaos/Makefile` against the buildroot sysroot +
`-ldbus-1 -lglib-2.0 -lbluetooth`).

## Stage into the image + autostart

After a successful build, copy the binary into the rootfs overlay and rebuild
the image:

```
cp apps/output/tuyaos_demo_wukong_ai_1.0.55/tuyaos_demo_wukong_ai \
   ../../buildroot/board/alientek/atk-dlrk3506/fs-overlay/usr/bin/
cd ../../.. && make buildroot && make updateimg   # then flash output/firmware/update.img
```

Boot autostart is already wired:
`../../buildroot/board/alientek/atk-dlrk3506/fs-overlay/etc/init.d/S98wukong_ai`
(launches from `/userdata/tuya_wukong`, log `/var/log/wukong_ai.log`).

## What's in this port

- `apps/src/boards/RK3506B_BOARD/` — RK3506B board
- `apps/build/appconfig/RK3506B_BOARD` (+ active `apps/build/tuya_app.config`)
- `apps/local.mk`, `apps/CMakeLists.txt` — `CONFIG_RK3506B_BOARD` wiring
- `apps/src/tuya_app_main.c` — PID/UUID/AUTHKEY (software auth)
- `apps/src/tuya_ai_toy.c` — `tkl_wakeup.h` guarded under `ENABLE_LOW_POWER`
- TKL adaptation (WiFi/BT/flash/stubs) in the vendor adapter — see
  `PORTING_RK3506B.md` for the full record, including runtime bring-up notes
  (Wi-Fi interface, wpa_supplicant, provisioning).
