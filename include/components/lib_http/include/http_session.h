/**
 * @file http_session.h
 * @brief http seesion
 * @version 0.1
 * @date 2024-09-29
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

#ifndef __HTTP_SESSION_H_
#define __HTTP_SESSION_H_

#include "tuya_cloud_types.h"
#include "httpc.h"
#include "http_inf.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief This API is used to set the global default session timeout
 *
 * @param[in] timeout_ms session timeout in millisecond
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET http_inf_client_set_session_timeout(IN UINT16_T timeout_ms);

/**
 * @brief This API is used to handle session
 *
 * @param[in] req http request
 * @param[in] url URL of HTTP request
 * @param[in] field_flags http header field flags
 * @param[in] callback Handler of HTTP response
 * @param[in] p_decode_key Decode key of HTTP content
 * @param[in] pri_data Private data used by HTTP request
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET http_inf_com_hanle_session_raw(IN CONST http_req_t *req,
                                           IN CONST CHAR_T *url,
                                           IN CONST http_hdr_field_sel_t field_flags,
                                           IN CONST HTTP_INF_CB callback,
                                           IN CONST CHAR_T *p_decode_key,
                                           INOUT PVOID_T *pri_data);

/**
 * @brief This API is used to enable http session
 *
 * @return OPRT_OK on success. Others on error, please refer to tuya_error_code.h
 */
OPERATE_RET http_inf_client_session_enable();
#ifdef __cplusplus
}
#endif

#endif
