/**
 * @file tuya_svc_astro_timer.h
 * @brief astro timer header file
 * @version 0.1
 * @date 2024-09-30
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

#ifndef __TUYA_SVC_ASTRO_TIMER__
#define __TUYA_SVC_ASTRO_TIMER__

#ifdef __cplusplus
extern "C" {
#endif

#include "ty_cJSON.h"
#include "tuya_cloud_types.h"

/**
 * @brief astro timer reset
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 *
 */
int tuya_astro_timer_reset(void);

/**
 * @brief init astro timer
 *
 * @param[in] data fix null
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 *
 */
OPERATE_RET tuya_astro_timer_init(VOID *data);
#ifdef __cplusplus
}
#endif
#endif


