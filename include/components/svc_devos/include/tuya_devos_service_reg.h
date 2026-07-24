/**
 * @file tuya_devos_service_reg.h
 * @brief tuya devos service register
 * @version 0.1
 * @date 2024-10-08
 *
 * @copyright Copyright (c) 2023 Tuya Inc. All Rights Reserved.
 *
 * Permission is hereby granted, to any person obtaining a copy of this software and
 * associated documentation files (the "Software"), Under the premise of complying
 * with the license of the third-party open source software contained in the software,
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software.
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "tuya_cloud_types.h"

#ifndef __TUYA_DEVOS_SERVICE_REG_H__
#define __TUYA_DEVOS_SERVICE_REG_H__

#define SERVICE_REG_NETCFG (0x1 << 0)
#define SERVICE_REG_WIFI   (0x1 << 1)
#define SERVICE_REG_WIRED  (0x1 << 2)
#define SERVICE_REG_BLE    (0x1 << 3)
#define SERVICE_REG_CLOUD  (0x1 << 4)
#define SERVICE_REG_USER   (0x1 << 5)

// service netcfg
#define SERVICE_REG_NETCFG_EZ           (SERVICE_REG_NETCFG | ((0x1 << 0) << 16))
#define SERVICE_REG_NETCFG_AP           (SERVICE_REG_NETCFG | ((0x1 << 1) << 16))
#define SERVICE_REG_NETCFG_FFS          (SERVICE_REG_NETCFG | ((0x1 << 2) << 16))
#define SERVICE_REG_NETCFG_PEGASUS      (SERVICE_REG_NETCFG | ((0x1 << 3) << 16))

// service wifi
#define SERVICE_REG_WIFI_PROTECT        (SERVICE_REG_WIFI | ((0x1 << 0) << 16))

// service cloud
#define SERVICE_REG_CLOUD_LOCALKEY      (SERVICE_REG_CLOUD | ((0x1 << 0) << 16))
#define SERVICE_REG_CLOUD_ASTRO_TIMER   (SERVICE_REG_CLOUD | ((0x1 << 1) << 16))

/**
 * @brief devos service reg callback
 *
 * @param[in] param: service param
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
typedef OPERATE_RET(*DEVOS_SERVICE_DO_CB)(VOID *param);

/**
 * @brief devos service reg init
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tuya_devos_service_init();

/**
 * @brief devos service reg
 *
 * @param[in] type: service type
 * @param[in] cb: service callback
 * @param[in] param: service param
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tuya_devos_service_reg(INT_T type, DEVOS_SERVICE_DO_CB cb, VOID *param);

/**
 * @brief devos service unreg
 *
 * @param[in] type: service type
 * @param[in] cb: service callback
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tuya_devos_service_unreg(INT_T type, DEVOS_SERVICE_DO_CB cb);

/**
 * @brief devos service reg handle
 *
 * @param[in] type: service type
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET tuya_devos_service_handle(INT_T type);

/**
 * @brief check if devos service reg
 *
 * @param[in] type: service type
 *
 * @return TRUE is reg, FALSE is not reg
 */
BOOL_T tuya_devos_service_is_reg(INT_T type);
#endif //__TUYA_DEVOS_SERVICE_REG_H__