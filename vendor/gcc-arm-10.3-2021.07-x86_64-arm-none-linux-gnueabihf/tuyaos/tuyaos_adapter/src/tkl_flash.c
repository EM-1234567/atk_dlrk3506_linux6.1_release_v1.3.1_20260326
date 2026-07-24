/**
 * @file tkl_flash.c
 * @brief TuyaOS flash TKL for ATK-DLRK3506B — file-backed.
 *
 * RK3506B boots from NAND/UBI; TuyaOS KV/MF regions are backed by a single
 * file under the persistent /userdata partition instead of raw flash. The
 * "flash address" maps 1:1 to a file offset.
 *
 * @copyright Copyright (c) 2024 Tuya Inc. All Rights Reserved.
 */
#include "tuya_cloud_types.h"
#include "tkl_flash.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>

#define TKL_FLASH_FILE  "/userdata/tuya_flash.bin"
#define TKL_FLASH_TOTAL (4 * 1024 * 1024)   /* 4 MiB backing store */
#define TKL_FLASH_BLKSZ (4 * 1024)          /* erase/write block (4 KiB) */

/*
 * Partition layout inside the 4 MiB backing file. Offsets/sizes are 4 KiB
 * aligned (block size). Mirrors the standard TuyaOS layout (KV_DATA 64 KiB,
 * KV_KEY 4 KiB, UF 64 KiB, KV_PROTECT 4 KiB) so the SDK's KV/simple_flash
 * layer can compute its region geometry. Addresses map 1:1 to file offsets.
 */
#define TKL_KV_KEY_ADDR     (0x00000)               /*   4 KiB */
#define TKL_KV_KEY_SIZE     (0x01000)
#define TKL_KV_DATA_ADDR    (0x01000)               /*  64 KiB */
#define TKL_KV_DATA_SIZE    (0x10000)
#define TKL_KV_SWAP_ADDR    (0x11000)               /*  64 KiB */
#define TKL_KV_SWAP_SIZE    (0x10000)
#define TKL_UF_ADDR         (0x21000)               /*  64 KiB */
#define TKL_UF_SIZE         (0x10000)
#define TKL_KV_PROTECT_ADDR (0x31000)               /*   4 KiB */
#define TKL_KV_PROTECT_SIZE (0x01000)
#define TKL_INFO_ADDR       (0x32000)               /*   4 KiB */
#define TKL_INFO_SIZE       (0x01000)

static int tkl_flash_fd(void)
{
    static int fd = -1;
    if (fd < 0) {
        mkdir("/userdata", 0755);
        fd = open(TKL_FLASH_FILE, O_RDWR | O_CREAT, 0644);
        if (fd >= 0) {
            struct stat st;
            if (fstat(fd, &st) == 0 && st.st_size < (off_t)TKL_FLASH_TOTAL) {
                /* Real erased flash reads 0xFF. Size the file and fill it with
                 * 0xFF once so the KV/simple_flash layer sees a clean empty
                 * flash on first boot (ftruncate alone would leave 0x00, which
                 * can be misread as corrupt rather than empty). One-time. */
                if (ftruncate(fd, TKL_FLASH_TOTAL) == 0) {
                    UCHAR_T pad[TKL_FLASH_BLKSZ];
                    UINT_T i;
                    memset(pad, 0xFF, sizeof(pad));
                    lseek(fd, 0, SEEK_SET);
                    for (i = 0; i < TKL_FLASH_TOTAL; i += sizeof(pad)) {
                        if (write(fd, pad, sizeof(pad)) <= 0) {
                            break;
                        }
                    }
                }
            }
        }
    }
    return fd;
}

OPERATE_RET tkl_flash_read(UINT32_T addr, UCHAR_T *dst, UINT32_T size)
{
    if (!dst) return OPRT_INVALID_PARM;
    int fd = tkl_flash_fd();
    if (fd < 0) return OPRT_COM_ERROR;
    UINT32_T off = 0;
    while (off < size) {
        ssize_t n = pread(fd, dst + off, size - off, addr + off);
        if (n <= 0) { memset(dst + off, 0xFF, size - off); break; }
        off += n;
    }
    return OPRT_OK;
}

OPERATE_RET tkl_flash_write(UINT32_T addr, CONST UCHAR_T *src, UINT32_T size)
{
    if (!src) return OPRT_INVALID_PARM;
    int fd = tkl_flash_fd();
    if (fd < 0) return OPRT_COM_ERROR;
    UINT32_T off = 0;
    while (off < size) {
        ssize_t n = pwrite(fd, src + off, size - off, addr + off);
        if (n <= 0) return OPRT_COM_ERROR;
        off += n;
    }
    return OPRT_OK;
}

OPERATE_RET tkl_flash_erase(UINT32_T addr, UINT32_T size)
{
    int fd = tkl_flash_fd();
    if (fd < 0) return OPRT_COM_ERROR;
    UCHAR_T *buf = (UCHAR_T *)malloc(size);
    if (!buf) return OPRT_RESOURCE_NOT_READY;
    memset(buf, 0xFF, size);
    UINT32_T off = 0;
    while (off < size) {
        ssize_t n = pwrite(fd, buf + off, size - off, addr + off);
        if (n <= 0) { free(buf); return OPRT_COM_ERROR; }
        off += n;
    }
    free(buf);
    return OPRT_OK;
}

OPERATE_RET tkl_flash_lock(UINT32_T addr, UINT32_T size)   { (void)addr; (void)size; return OPRT_OK; }
OPERATE_RET tkl_flash_unlock(UINT32_T addr, UINT32_T size) { (void)addr; (void)size; return OPRT_OK; }

OPERATE_RET tkl_flash_get_one_type_info(TUYA_FLASH_TYPE_E type, TUYA_FLASH_BASE_INFO_T *info)
{
    if (!info) {
        return OPRT_INVALID_PARM;
    }
    memset(info, 0, sizeof(*info));
    info->partition_num = 1;
    info->partition[0].block_size = TKL_FLASH_BLKSZ;

    switch (type) {
    case TUYA_FLASH_TYPE_KV_KEY:
        info->partition[0].start_addr = TKL_KV_KEY_ADDR;
        info->partition[0].size       = TKL_KV_KEY_SIZE;
        break;
    case TUYA_FLASH_TYPE_KV_DATA:
        info->partition[0].start_addr = TKL_KV_DATA_ADDR;
        info->partition[0].size       = TKL_KV_DATA_SIZE;
        break;
    case TUYA_FLASH_TYPE_KV_SWAP:
        info->partition[0].start_addr = TKL_KV_SWAP_ADDR;
        info->partition[0].size       = TKL_KV_SWAP_SIZE;
        break;
    case TUYA_FLASH_TYPE_UF:
        info->partition[0].start_addr = TKL_UF_ADDR;
        info->partition[0].size       = TKL_UF_SIZE;
        break;
    case TUYA_FLASH_TYPE_KV_PROTECT:
        info->partition[0].start_addr = TKL_KV_PROTECT_ADDR;
        info->partition[0].size       = TKL_KV_PROTECT_SIZE;
        break;
    case TUYA_FLASH_TYPE_INFO:
        info->partition[0].start_addr = TKL_INFO_ADDR;
        info->partition[0].size       = TKL_INFO_SIZE;
        break;
    default:
        /* Partition type not provided on this platform. */
        return OPRT_INVALID_PARM;
    }
    return OPRT_OK;
}
