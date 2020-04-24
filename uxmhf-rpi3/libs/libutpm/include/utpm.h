/*
 * @UBERXMHF_LICENSE_HEADER_START@
 *
 * uber eXtensible Micro-Hypervisor Framework (Raspberry Pi)
 *
 * Copyright 2018 Carnegie Mellon University. All Rights Reserved.
 *
 * NO WARRANTY. THIS CARNEGIE MELLON UNIVERSITY AND SOFTWARE ENGINEERING
 * INSTITUTE MATERIAL IS FURNISHED ON AN "AS-IS" BASIS. CARNEGIE MELLON
 * UNIVERSITY MAKES NO WARRANTIES OF ANY KIND, EITHER EXPRESSED OR IMPLIED,
 * AS TO ANY MATTER INCLUDING, BUT NOT LIMITED TO, WARRANTY OF FITNESS FOR
 * PURPOSE OR MERCHANTABILITY, EXCLUSIVITY, OR RESULTS OBTAINED FROM USE OF
 * THE MATERIAL. CARNEGIE MELLON UNIVERSITY DOES NOT MAKE ANY WARRANTY OF
 * ANY KIND WITH RESPECT TO FREEDOM FROM PATENT, TRADEMARK, OR COPYRIGHT
 * INFRINGEMENT.
 *
 * Released under a BSD (SEI)-style license, please see LICENSE or
 * contact permission@sei.cmu.edu for full terms.
 *
 * [DISTRIBUTION STATEMENT A] This material has been approved for public
 * release and unlimited distribution.  Please see Copyright notice for
 * non-US Government use and distribution.
 *
 * Carnegie Mellon is registered in the U.S. Patent and Trademark Office by
 * Carnegie Mellon University.
 *
 * @UBERXMHF_LICENSE_HEADER_END@
 */

/*
 * Author: Amit Vasudevan (amitvasudevan@acm.org)
 *
 */

/*
 * modified for uxmhf-rpi3 by amit vasudevan (amitvasudevan@acm.org)
 */


/**
 * Common type and structure definitions across TrustVisor's internal
 * Micro-TPM implementation, tee-sdk's svcapi, and any PAL writer who
 * uses any of the relevant Micro-TPM operations.
 */

#ifndef __UTPM_H__
#define __UTPM_H__

/**
 * FIXME: Once libemhfcrypto exists, it may be reasonable to depend on
 * rsa.h in here.  For now, it's not.
 */
//#include <rsa.h>
#include <types.h>

/* Intentionally not including basic types such as uintXX_t, since the
 * headers that provide these may vary across hypervisor-internal
 * environments and test userspace builds.  Whatever code consumes
 * this header is expected to have included the appropriate basic
 * types already. */

/* FIXME: A lot of these values are also defined in the public header
 * files for a TSS.  We should consider leveraging those and changing
 * the names here of the ones where we break compatibility. */

/* constant value for TPM chip */
#define TPM_RSA_KEY_LEN                256 /* RSA key size is 2048 bit */
#define TPM_HASH_SIZE                  20

#define  MAX_PCR_SEL_NUM 4
#define  MAX_PCR_SEL_SIZE (4+4*MAX_PCR_SEL_NUM)
#define  MAX_PCR_DATA_SIZE (MAX_PCR_SEL_NUM*20)

#define		MAX_PCR_COMPOSITE_SIZE	256
#define		MAX_TPM_SEAL_DATA_SIZE	1124	//1K of data + 100 bytes of meta-data added by seal

#define  MAX_TPM_EXTEND_DATA_LEN 4096
#define  MAX_TPM_RAND_DATA_LEN 4096

#define TPM_QUOTE_SIZE ( 8 + MAX_PCR_SEL_SIZE + MAX_PCR_DATA_SIZE + TPM_NONCE_SIZE + TPM_RSA_KEY_LEN )

#define TPM_CONFOUNDER_SIZE 20

/* Return codes for uTPM operations. */
#define UTPM_SUCCESS 0
#define UTPM_ERR_BAD_PARAM 1
#define UTPM_ERR_PCR_OUT_OF_RANGE 2
#define UTPM_ERR_INSUFFICIENT_ENTROPY 3
#define UTPM_ERR 4

/* MicroTPM related definitions */
#define TPM_PCR_SIZE                   20
#define TPM_AES_KEY_LEN                128 /* key size is 128 bit */
#define TPM_AES_KEY_LEN_BYTES (TPM_AES_KEY_LEN/8)
#define TPM_HMAC_KEY_LEN               20

/* max len of sealed data */
#define  MAX_SEALDATA_LEN 2048
#define  SEALDATA_HEADER_LEN 48

/* TODO: create an enum with meaningful errors that can be passed back
 * to PAL authors. */
typedef uint32_t TPM_RESULT; 

#define TPM_PCR_NUM                    8

struct tdTPM_PCR_SELECTION { 
    uint16_t sizeOfSelect;            /* The size in bytes of the pcrSelect structure */
    uint8_t pcrSelect[TPM_PCR_NUM/8]; /* This SHALL be a bit map that indicates if a PCR
                                            is active or not */
} __attribute__((packed));

typedef struct tdTPM_PCR_SELECTION TPM_PCR_SELECTION; 

#define TPM_MAX_QUOTE_LEN ( \
    sizeof(TPM_PCR_SELECTION) + sizeof(uint32_t) \
    + TPM_PCR_NUM*TPM_HASH_SIZE /* max size of TPM_PCR_COMPOSITE */ \
    + sizeof(uint32_t)          /* sigSize */ \
    + TPM_RSA_KEY_LEN)          /* sig */



typedef struct tdTPM_DIGEST{
  uint8_t value[TPM_HASH_SIZE];
} TPM_DIGEST;

typedef TPM_DIGEST TPM_COMPOSITE_HASH;

typedef struct tdTPM_PCR_INFO { 
    TPM_PCR_SELECTION pcrSelection;      /* This SHALL be the selection of PCRs to which the
                                            data or key is bound. */
    TPM_COMPOSITE_HASH digestAtRelease;  /* This SHALL be the digest of the PCR indices and
                                            PCR values to verify when revealing Sealed Data
                                            or using a key that was wrapped to PCRs.  NOTE:
                                            This is passed in by the host, and used as
                                            authorization to use the key */
    TPM_COMPOSITE_HASH digestAtCreation; /* This SHALL be the composite digest value of the
                                            PCR values, at the time when the sealing is
                                            performed. NOTE: This is generated at key
                                            creation, but is just informative to the host,
                                            not used for authorization */
} TPM_PCR_INFO; 

#define ALIGN_UP(n, boundary)	(((n)+((boundary)-1))&(~((boundary)-1)))

#define TPM_NONCE_SIZE 20
typedef struct tdTPM_NONCE{
  uint8_t nonce[TPM_NONCE_SIZE];
} TPM_NONCE;

/* structure for storage */ /* XXX inconsistent with hardware TPM */
typedef struct tdTPM_SEALED_DATA{
  /*TPM_HMAC hmac;*/ /* NOT SURE HMAC */
  uint32_t dataSize;        
  uint8_t* data;        /*data to be sealed*/
} TPM_SEALED_DATA;

typedef struct tdTPM_STRUCT_VER{
  uint8_t major;   /* 0x01 */
  uint8_t minor;   /* 0x01 */
  uint8_t revMajor; /* 0x00 */
  uint8_t revMinor; /* 0x00 */
} TPM_STRUCT_VER;

typedef struct tdTPM_QUOTE_INFO{
  TPM_STRUCT_VER version;  /* must be 1.1.0.0 based on TPM part2 structure */
  uint8_t fixed[4];           /* this always be the string 'QUOT'*/
  TPM_COMPOSITE_HASH digestValue; 
  TPM_NONCE externalData;
} TPM_QUOTE_INFO;

/**
 * Everything below is specific to the internals of a MicroTPM
 * implementation, and as such is _not_ required by your average PAL
 * author or tee-sdk's svcapi.
 *
 * TODO: Refactor this file into something more sensible.
 */

typedef struct tdTPM_STORED_DATA{ /* XXX inconsistent with hardware TPM */
  uint32_t sealInfoSize;
  TPM_PCR_INFO sealInfo;       /* structure of TPM_PCR_INFO */
  uint32_t encDataSize;
  uint8_t* encData;  /* encrypted TPM_SEALED_DATA structure containg the confidential part of data*/
} TPM_STORED_DATA;

typedef struct utpm_master_state {
	TPM_DIGEST pcr_bank[TPM_PCR_NUM];
} __attribute__ ((packed)) utpm_master_state_t;


// utpm common functions
unsigned int utpm_seal_output_size(unsigned int inlen, bool usingPcrs);
bool utpm_pcr_is_selected(TPM_PCR_SELECTION *tpmsel, uint32_t i);
void utpm_pcr_select_i(TPM_PCR_SELECTION *tpmsel, uint32_t i);





/* TPM functions  */
TPM_RESULT utpm_pcrread(TPM_DIGEST* pcr_value,
                        utpm_master_state_t *utpm, uint32_t pcr_num);
TPM_RESULT utpm_extend(TPM_DIGEST *measurement, utpm_master_state_t *utpm, uint32_t pcr_num);

TPM_RESULT utpm_seal(utpm_master_state_t *utpm,
                     TPM_PCR_INFO *tpmPcrInfo,
                     uint8_t* input, uint32_t inlen,
                     uint8_t* output, uint32_t* outlen);
TPM_RESULT utpm_unseal(utpm_master_state_t *utpm, uint8_t* input, uint32_t inlen, uint8_t* output, uint32_t* outlen, TPM_COMPOSITE_HASH *digestAtCreation);

TPM_RESULT utpm_quote(TPM_NONCE* externalnonce, TPM_PCR_SELECTION* tpmsel, /* hypercall inputs */
                      uint8_t* output, uint32_t* outlen, /* hypercall outputs */
                      uint8_t* pcrComp, uint32_t* pcrCompLen,                      
                      utpm_master_state_t *utpm); /* TrustVisor inputs */

/**
 * Keeping these around solely for the Apache SSL web server demo
 */
TPM_RESULT utpm_seal_deprecated(uint8_t* pcrAtRelease, uint8_t* input, uint32_t inlen, uint8_t* output, uint32_t* outlen);
TPM_RESULT utpm_unseal_deprecated(utpm_master_state_t *utpm, uint8_t* input, uint32_t inlen, uint8_t* output, uint32_t* outlen);
TPM_RESULT utpm_quote_deprecated(uint8_t* externalnonce, uint8_t* output, uint32_t* outlen,
                      utpm_master_state_t *utpm, uint8_t* tpmsel, uint32_t tpmsel_len);


TPM_RESULT utpm_rand(uint8_t* buffer, uint32_t *numbytes);

/* return public key in PKCS #1 v2.1 (ASN.1 DER) */
TPM_RESULT utpm_id_getpub(uint8_t *N, uint32_t *len);

void utpm_init_instance(utpm_master_state_t *utpm);

/**
 * FIXME: Once libemhfcrypto exists, it may be reasonable to depend on
 * rsa.h in here.  For now, it's not.
 */
TPM_RESULT utpm_init_master_entropy(uint8_t *aeskey,
                                    uint8_t *hmackey,
                                    void /*rsa_context*/ *rsa);

#endif /* __UTPM_H__ */
