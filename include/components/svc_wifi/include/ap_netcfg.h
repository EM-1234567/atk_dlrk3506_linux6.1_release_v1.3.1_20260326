/**
 * @file ap_netcfg.h
 * @brief ap config
 * @version 0.1
 * @date 2024-10-14
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

#ifndef _AP_NETCFG_H_
#define _AP_NETCFG_H_

#include "tuya_cloud_types.h"
#include "tuya_cloud_com_defs.h"
#include "tuya_cloud_wifi_defs.h"
#include "netcfg_module.h"
#include "sdk_version.h"
#include "tuya_tls.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief ap netcfg init
 *
 * @param[in] data netcfg policy type
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET ap_netcfg_init(VOID *data);

/**
 * @brief send ack to app
 *
 * @param[in] frame_type type
 * @param[in] ret_code code
 * @param[in] p_data data
 * @param[in] data_len date len
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET ap_lan_send_ack(UINT_T frame_type, UINT_T ret_code, BYTE_T *p_data, UINT_T data_len);
#ifdef __cplusplus
}
#endif
#endif
