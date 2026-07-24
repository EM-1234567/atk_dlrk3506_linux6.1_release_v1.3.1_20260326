/**
 * @file ffs_netcfg.h
 * @brief ffs net config
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

#ifndef __FFS_NETCFG__
#define __FFS_NETCFG__
typedef OPERATE_RET(*FN_FFS_NET_CFG_CB)(IN CONST CHAR_T *ssid, IN CONST CHAR_T *passwd, IN CONST CHAR_T *token);

/**
 * @brief ffs netcfg init
 *
 * @param[in] data: netcfg policy type
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET ffs_netcfg_init(VOID *data);
#endif
