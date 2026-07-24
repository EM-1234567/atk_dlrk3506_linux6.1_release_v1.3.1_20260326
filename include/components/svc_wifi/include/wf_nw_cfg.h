/**
 * @file wf_nw_cfg.h
 * @brief smart config
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

#ifndef _WF_NW_CFG_H
#define _WF_NW_CFG_H

#include "tuya_cloud_types.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifdef  __WF_NW_CFG_GLOBALS
#define __WF_NW_CFG_EXT
#else
#define __WF_NW_CFG_EXT extern
#endif

/**
 * @brief smart cfg netcfg init
 *
 * @param[in] data: netcfg policy type
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET smart_netcfg_init(VOID *data);
#ifdef __cplusplus
}
#endif
#endif

