# tuyaos_demo_wukong_ai (RK3506B port)

App **source + vendor platform both live here** (`atk/app/tuyaos_demo_wukong_ai/`).
They build for ATK-DLRK3506B (32-bit armhf) via the TuyaOS framework, reached
through two symlinks:

```
../../tuya_os_sdk/RK3506B_TuyaOS-3.14.0/software/TuyaOS/apps/tuyaos_demo_wukong_ai
        └── symlink -> this directory            (app source)
../../tuya_os_sdk/RK3506B_TuyaOS-3.14.0/software/TuyaOS/vendor/gcc-arm-10.3-2021.07-x86_64-arm-none-linux-gnueabihf
        └── symlink -> this directory/vendor/gcc-arm-10.3-2021.07-x86_64-arm-none-linux-gnueabihf   (vendor platform)
```

The vendor's own `Makefile` was self-locating (`ROOT_DIR=$(abspath ../../../)`
+ `-include ../../../build/build_param` + inherited `CXX`), which resolves to
the **physical atk path** through the symlink and breaks the link. So that
Makefile (here, under `vendor/.../tuyaos/Makefile`) is patched to hardcode the
TuyaOS root and the armhf cross-toolchain (`ROOT_DIR`, `-include`, `CC`, `CXX`).

## Layout

```
app/tuyaos_demo_wukong_ai/
  src/, local.mk, CMakeLists.txt …            app source (RK3506B board, creds, etc.)
  vendor/gcc-arm-10.3-…-gnueabihf/             vendor platform (relocated from TuyaOS tree)
    tuyaos/tuyaos_adapter/src/tkl_*.c          TKL adapter — incl. my WiFi/BT/flash/stubs adaptations
    tuyaos/Makefile                            PATCHED: ROOT_DIR + CC/CXX hardcoded (see above)
    toolchain/                                 armhf gcc (~1.6 GB, re-downloadable)
  build_rk3506b.sh                             build entry (run from here)
  output/tuyaos_demo_wukong_ai_1.0.55/         produced binary
```

## Build

```
cd app/tuyaos_demo_wukong_ai
./build_rk3506b.sh            # -> output/tuyaos_demo_wukong_ai_1.0.55/tuyaos_demo_wukong_ai
```

`build_rk3506b.sh` runs `build_app.sh` in the TuyaOS tree (which compiles THIS
source via the symlink). The vendor platform (toolchain + internal headers +
`libtuyaos.a`) is downloaded once into `tuya_os_sdk/.../TuyaOS/vendor/` (~373 MB).

## Stage into the image + autostart

After a successful build, copy the binary into the rootfs overlay and rebuild
the image:

```
cp output/tuyaos_demo_wukong_ai_1.0.55/tuyaos_demo_wukong_ai \
   ../../buildroot/board/alientek/atk-dlrk3506/fs-overlay/usr/bin/
./build.sh        # at SDK root, then flash
```

Boot autostart is already wired:
`../../buildroot/board/alientek/atk-dlrk3506/fs-overlay/etc/init.d/S98wukong_ai`
(launches from `/userdata/tuya_wukong`, log `/var/log/wukong_ai.log`).

## What's in this port

- `src/boards/RK3506B_BOARD/` — RK3506B board
- `build/appconfig/RK3506B_BOARD` (+ active `build/tuya_app.config`)
- `local.mk`, `CMakeLists.txt` — `CONFIG_RK3506B_BOARD` wiring
- `src/tuya_app_main.c` — PID/UUID/AUTHKEY (software auth)
- `src/tuya_ai_toy.c` — `tkl_wakeup.h` guarded under `ENABLE_LOW_POWER`
- TKL adaptation (WiFi/BT/flash/stubs) in the vendor adapter — see
  `PORTING_RK3506B.md` (in the symlinked TuyaOS app dir) for the full record,
  including runtime bring-up notes (Wi-Fi interface, wpa_supplicant, provisioning).
