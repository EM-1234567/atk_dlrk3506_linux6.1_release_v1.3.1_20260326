/**
 * @file tal_aes_ext.h
 * @brief TAL AES-GCM authenticated encryption/decryption with hardware-internal key.
 *
 *  Mirrors tkl_aes_gcm_encrypt/decrypt (no key parameter). TAL never holds key material.
 *
 *  Requires ENABLE_PLATFORM_AES_EXT; otherwise returns OPRT_NOT_SUPPORTED.
 *
 *  Error convention:
 *      OPRT_OK                       Success (decrypt: TAG verified)
 *      OPRT_AUTHENTICATION_FAIL      GCM TAG verification failed
 *      OPRT_NOT_SUPPORTED            ENABLE_PLATFORM_AES_EXT not enabled
 *      OPRT_INVALID_PARM             Invalid parameter
 *      OPRT_COM_ERROR                Other hardware error
 *
 *  Key is never passed as a parameter — derived internally by hardware.
 */

#ifndef __TAL_AES_EXT_H__
#define __TAL_AES_EXT_H__

#include "tuya_cloud_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief  AES-GCM authenticated encryption (key derived internally).
 *
 * @param[in]  iv         IV (nonce), 12 bytes recommended
 * @param[in]  iv_len     IV length in bytes
 * @param[in]  add        Additional authenticated data, may be NULL
 * @param[in]  add_len    AAD length in bytes
 * @param[in]  input      Plaintext input
 * @param[in]  length     Plaintext length in bytes
 * @param[out] output     Ciphertext output (size >= length)
 * @param[out] tag        GCM authentication TAG output
 * @param[in]  tag_len    TAG length in bytes, 16 recommended
 *
 * @return OPRT_OK on success; others on failure
 */
OPERATE_RET tal_aes_gcm_encrypt(
    CONST UINT8_T *iv,
    SIZE_T         iv_len,
    CONST UINT8_T *add,
    SIZE_T         add_len,
    CONST UINT8_T *input,
    UINT8_T       *output,
    SIZE_T         length,
    UINT8_T       *tag,
    SIZE_T         tag_len
);

/**
 * @brief  AES-GCM authenticated decryption (key derived internally).
 *
 *  On TAG verification failure, output content is untrustworthy.
 *
 * @param[in]  iv         IV (nonce)
 * @param[in]  iv_len     IV length in bytes
 * @param[in]  add        Additional authenticated data, may be NULL
 * @param[in]  add_len    AAD length in bytes
 * @param[in]  input      Ciphertext input
 * @param[in]  length     Ciphertext length in bytes
 * @param[out] output     Plaintext output (size >= length)
 * @param[in]  tag        GCM authentication TAG to verify
 * @param[in]  tag_len    TAG length in bytes
 *
 * @return OPRT_OK on success with TAG verified; OPRT_AUTHENTICATION_FAIL on failure
 */
OPERATE_RET tal_aes_gcm_decrypt(
    CONST UINT8_T *iv,
    SIZE_T         iv_len,
    CONST UINT8_T *add,
    SIZE_T         add_len,
    CONST UINT8_T *input,
    UINT8_T       *output,
    SIZE_T         length,
    CONST UINT8_T *tag,
    SIZE_T         tag_len
);

#ifdef __cplusplus
}
#endif

#endif /* __TAL_AES_EXT_H__ */
