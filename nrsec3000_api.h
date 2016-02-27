/*
 * nrsec3000_api.h
 *
 *  Created on: May 5, 2015
 *      Author: bqin
 */

#ifndef NRSEC3000_API_H_
#define NRSEC3000_API_H_


extern void nrsec3000_init(void);
extern int  nrsec3000_SM1_import_key(uint8_t *key);
extern int  nrsec3000_SM1_import_IV(uint8_t *IV);
extern int  nrsec3000_SM1_encrypt(uint8_t *in, uint8_t *out, uint16_t len, uint16_t *out_len);
extern int  nrsec3000_SM1_decrypt(uint8_t *in, uint8_t *out, uint16_t len, uint16_t *out_len);
extern int  nrsec3000_SM2_gen_key(uint8_t index);
extern int  nrsec3000_SM2_export_pubKey(uint8_t *key, int index);
extern int  nrsec3000_SM2_export_priKey(uint8_t *key, int index);
extern int  nrsec3000_SM2_import_pubKey(uint8_t *key, int index);
extern int  nrsec3000_SM2_import_priKey(uint8_t *key, int index);
extern int  nrsec3000_SM3_Hash(uint8_t *in, uint32_t inl, uint8_t *out);
extern int  nrsec3000_SM2_Sign(uint8_t *in, uint32_t inl, uint8_t *out, int index);
extern int  nrsec3000_SM2_CheckSign(uint8_t *hash, uint8_t *sign, int index);
extern int  nrsec3000_SM2_encrypt(uint8_t *in, uint32_t inl, uint8_t *out, int index);
extern int  nrsec3000_SM2_decrypt(uint8_t *in, uint32_t inl, uint8_t *out, int index);
extern int  nrsec3000_SM2_gen_CertRequest(uint8_t *in, uint32_t inl, uint8_t *out, uint16_t *outl, int format, int index);
extern int  nrsec3000_gen_random(uint8_t *out, uint8_t len);
extern int  nrsec3000_get_version(uint8_t *out);
extern int  nrsec3000_security_verify(uint8_t *in, uint8_t *out);

extern int  sm3(uint8_t *in,int inl,uint8_t *out, uint8_t *pubkey, uint8_t *pucID, int idl);

#endif /* NRSEC3000_API_H_ */
