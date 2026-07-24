/** @file tuya_g711_utils.h
 * @brief g711 encode and decode utility
 */
#ifndef __TUYA_G711_UTILS_H__
#define __TUYA_G711_UTILS_H__

#ifdef __cplusplus
extern "C" {
#endif


#define		TUYA_G711_A_LAW     (0)
#define		TUYA_G711_MU_LAW    (1)

typedef struct {
    unsigned char type;    // TUYA_G711_A_LAW or TUYA_G711_MU_LAW
    unsigned short *src;   // input data
    unsigned int src_len;  // input len
    unsigned char *drc;    // out data
    unsigned int *p_out;   // out len
} TY_AUDIO_G711_INFO_T;

#if 1
/** @brief encode 8K pcm to g711
 * @param[in]  type    TUYA_G711_A_LAW or TUYA_G711_MU_LAW
 * @param[in]  src     pcm data
 * @param[in]  src_len  size of pcm data
 * @param[in]  drc     g711 out data
 * @param[in]  p_out   size of g711 out data
 * @return error code
 * - 0  success
 * - -1 fail
 */
int tuya_g711_encode(unsigned char type, unsigned short *src, unsigned int src_len, unsigned char *drc, unsigned int *p_out);

/** @brief decode 8K g711 to pcm
 * @param[in]  type    TUYA_G711_A_LAW or TUYA_G711_MU_LAW
 * @param[in]  src     g711 data
 * @param[in]  src_len  size of g711 data
 * @param[in]  drc     pcm out data
 * @param[in]  p_out   size of pcm out data
 * @return error code
 * - 0 success
 * - -1 fail
 */
int tuya_g711_decode(unsigned char type, unsigned short *src, unsigned int src_len, unsigned char *drc, unsigned int *p_out);

/** @brief encode 16K pcm to g711, 只允许单路音频转换，逐渐废弃, 推荐使用v2接口
 * @param[in]  type    TUYA_G711_A_LAW or TUYA_G711_MU_LAW
 * @param[in]  src     pcm data
 * @param[in]  src_len  size of pcm data
 * @param[in]  drc     g711 out data
 * @param[in]  p_out   size of g711 out data
 * @return error code
 * - 0  success
 * - -1 fail
 */
int tuya_g711_encode_16K(unsigned char type, unsigned short *src, unsigned int src_len, unsigned char *drc, unsigned int *p_out);

/** @brief decode g711 to 16k pcm, 只允许单路音频转换，逐渐废弃, 推荐使用v2接口
 * @param[in]  type    TUYA_G711_A_LAW or TUYA_G711_MU_LAW
 * @param[in]  src     g711 data
 * @param[in]  src_len  size of g711 data
 * @param[out]  drc     16k pcm out data
 * @param[out]  p_out   size of pcm out data
 * @return error code
 * - 0  success
 * - -1 fail
 */
int tuya_g711_decode_16K(unsigned char type, unsigned char *src, unsigned int src_len, unsigned short *drc, unsigned int *p_out);

/** @brief decode g711 to 16k pcm 
 * @param[in] **pphandle, 由底层分配，首次传入时为NULL, p_handle需保存,以便后续使用传入
 * @param[in] pinfo
 * @return error code
 * - 0  success
 * - -1 fail
 */
int tuya_g711_encode_16K_v2(void **p_handle, TY_AUDIO_G711_INFO_T *pinfo);

/** @brief decode g711 to 16k pcm
 * @param[in] *p_handle, 由底层分配，首次传入时为NULL, p_handle需保存,以便后续使用传入
 * @param[in]  pinfo
 * @return error code
 * - 0  success
 * - -1 fail
 */
int tuya_g711_decode_16K_v2(void **p_handle, TY_AUDIO_G711_INFO_T *pinfo);

/** @brief free p_handle
 * @param[in] p_handle; 
 * @return error code
 * - 0  success
 * - -1 fail
 */
int tuya_g711_encode_16K_v2_release(void *p_handle);

/** @brief free p_handle
 * @param[in] p_handle; 
 * @return error code
 * - 0  success
 * - -1 fail
 */
int tuya_g711_decode_16K_v2_release(void *p_handle);

#else

void tuya_g711_encode(unsigned int type, unsigned int sample_num, unsigned char *samples, unsigned char *bitstream);

void tuya_g711_encode_16k(unsigned int type, unsigned int sample_num, unsigned char *samples, unsigned char *bitstream);

void tuya_g711_decode(unsigned int type, unsigned int sample_num, unsigned char *samples, unsigned char *bitstream);

#endif
#ifdef __cplusplus
}
#endif

#endif // TUYA_G711_UTILS_H
