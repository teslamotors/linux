/*
 * drivers/video/tegra/dc/tsec_drv.h
 *
 * Copyright (c) 2014, NVIDIA CORPORATION, All rights reserved.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef TSEC_DRV_H
#define TSEC_DRV_H
/*
   CLASS NV_95A1_TSEC
   ===================
*/

/*
 * HDCP
 *
 * Method parameters for HDCP 2.X methods. Also
 * provides status signing, ksv list validation, srm validation and stream
 * validation for HDCP 1.X. Uses HDCP2.0/HDCP2.1 spec.
 * Glossary at end of the file.
 */
/* Size in bytes */
#define HDCP_SIZE_RECV_ID_8             (40/8)
#define HDCP_SIZE_RTX_8                 (64/8)
#define HDCP_SIZE_RTX_64                (HDCP_SIZE_RTX_8/8)
#define HDCP_SIZE_RRX_8                 (64/8)
#define HDCP_SIZE_RRX_64                (HDCP_SIZE_RRX_8/8)
#define HDCP_SIZE_RN_8                  (64/8)
#define HDCP_SIZE_RN_64                 (HDCP_SIZE_RTX_8/8)
#define HDCP_SIZE_RIV_8                 (64/8)
#define HDCP_SIZE_RIV_64                (HDCP_SIZE_RIV_8/8)
#define HDCP_SIZE_CERT_RX_8             (4176/8)
#define HDCP_SIZE_DCP_KPUB_8            (3072/8)
#define HDCP_SIZE_DCP_KPUB_64           (HDCP_SIZE_DCP_KPUB_8/8)
#define HDCP_SIZE_RX_KPUB_8             (1048/8)
/*each receiver ID is 5 bytes long * 128 max receivers */
#define HDCP_SIZE_MAX_RECV_ID_LIST_8    640
#define HDCP_SIZE_MAX_RECV_ID_LIST_64   (HDCP_SIZE_MAX_RECV_ID_LIST_8/8)
#define HDCP_SIZE_PVT_RX_KEY_8          (340)
#define HDCP_SIZE_TMR_INFO_8            (5)
#define HDCP_SIZE_RCV_INFO_8            (5)
#define HDCP_SIZE_KM_8                  (16)
#define HDCP_SIZE_DKEY_8                (16)
#define HDCP_SIZE_KD_8                  (32)
#define HDCP_SIZE_KH_8                  (16)
#define HDCP_SIZE_KS_8                  (16)
#define HDCP_SIZE_M_8                   (128/8)
#define HDCP_SIZE_M_64                  (HDCP_SIZE_M_8/8)
#define HDCP_SIZE_E_KM_8                (1024/8)
#define HDCP_SIZE_E_KM_64               (HDCP_SIZE_E_KM_8/8)
#define HDCP_SIZE_EKH_KM_8              (128/8)
#define HDCP_SIZE_EKH_KM_64             (HDCP_SIZE_EKH_KM_8/8)
#define HDCP_SIZE_E_KS_8                (128/8)
#define HDCP_SIZE_E_KS_64               (HDCP_SIZE_E_KS_8/8)
/* 96 bytes,  round up from 85 bytes */
#define HDCP_SIZE_EPAIR_8               96
#define HDCP_SIZE_EPAIR_64              (HDCP_SIZE_EPAIR_8/8)
#define HDCP_SIZE_EPAIR_SIGNATURE_8     (256/8)
#define HDCP_SIZE_EPAIR_SIGNATURE_64    (HDCP_SIZE_EPAIR_SIGNATURE_8/8)
#define HDCP_SIZE_HPRIME_8              (256/8)
#define HDCP_SIZE_HPRIME_64             (HDCP_SIZE_HPRIME_8/8)
#define HDCP_SIZE_LPRIME_8              (256/8)
#define HDCP_SIZE_LPRIME_64             (HDCP_SIZE_LPRIME_8/8)
#define HDCP_SIZE_MPRIME_8              (256/8)
#define HDCP_SIZE_MPRIME_64             (HDCP_SIZE_MPRIME_8/8)
#define HDCP_SIZE_VPRIME_2X_8           (256/8)
#define HDCP_SIZE_VPRIME_2X_64          (HDCP_SIZE_VPRIME_2X_8/8)
#define HDCP_SIZE_SPRIME_8              384
#define HDCP_SIZE_SPRIME_64             (HDCP_SIZE_SPRIME_8/8)
#define HDCP_SIZE_SEQ_NUM_V_8           3
#define HDCP_SIZE_SEQ_NUM_M_8           3
#define HDCP_SIZE_CONTENT_ID_8          2
#define HDCP_SIZE_CONTENT_TYPE_8        1
#define HDCP_SIZE_PES_HDR_8             (128/8)
#define HDCP_SIZE_PES_HDR_64            (HDCP_SIZE_PES_HDR_8/8)
#define HDCP_SIZE_CHIP_NAME             (8)
#define HDCP_VERIFY_VPRIME_MAX_ATTEMPTS 3
/* HDCP1X uses SHA1 for VPRIME which produces 160 bits of output */
#define HDCP_SIZE_VPRIME_1X_8           (160/8)
#define HDCP_SIZE_VPRIME_1X_32          (HDCP_SIZE_VPRIME_1X_8/4)
#define HDCP_SIZE_LPRIME_1X_8           (160/8)
#define HDCP_SIZE_LPRIME_1X_32          (HDCP_SIZE_LPRIME_1X_8/4)
#define HDCP_SIZE_QID_1X_8              (64/8)
#define HDCP_SIZE_QID_1X_64             (HDCP_SIZE_QID_1X_8/8)
/* Constants
 * Changing this contant will change size of certain structures below
 * Please make sure they are resized accordingly.
 */
#define HDCP_MAX_STREAMS_PER_RCVR        2
/* HDCP versions */
#define HDCP_VERSION_1X                                  (0x00000001)
#define HDCP_VERSION_20                                  (0x00000002)
#define HDCP_VERSION_21                                  (0x00000003)
#define HDCP_VERSION_22                                  (0x00000004)
/* COMMON ERROR CODES */
#define HDCP_ERROR_UNKNOWN                               (0x80000000)
#define HDCP_ERROR_NONE                                  (0x00000000)
#define HDCP_ERROR_INVALID_SESSION                       (0x00000001)
#define HDCP_ERROR_SB_NOT_SET                            (0x00000002)
#define HDCP_ERROR_NOT_INIT                              (0x00000003)
#define HDCP_ERROR_INVALID_STAGE                         (0x00000004)
#define HDCP_ERROR_MSG_UNSUPPORTED                       (0x00000005)
/*
 * READ_CAPS
 *
 * This method passes the HDCP Tsec application's capabilities back to the
 * client. The capabilities include supported versions, maximum number of
 * simultaneous sessions supported, exclusive dmem support, max scratch
 * buffer needed etc.If DMEM carveout (exclusive DMEM) is not available for
 * HDCP, then the client must allocate a scratch buffer of size 'requiredScratch
 * BufferSize' in FB and pass it TSEC.
 *
 * Depends on: [none]
 */
struct hdcp_read_caps_param {
	unsigned int  supported_versions_mask;               /* >>out */
	unsigned int  max_sessions;                          /* >>out */
	unsigned int  max_active_sessions;                   /* >>out */
	unsigned int  scratch_buffer_size;                   /* >>out */
	unsigned int  max_streams_per_receiver;              /* >>out */
	unsigned int  current_build_mode;                    /* >>out */
	unsigned int  falcon_ip_ver;                         /* >>out */
	unsigned char   b_is_rcv_supported;                  /* >>out */
	unsigned char   reserved[3];
	unsigned char   chip_name[HDCP_SIZE_CHIP_NAME];      /* >>out */
	unsigned char   b_is_stack_track_enabled;            /* >>out */
	unsigned char   b_is_imem_track_enabled;             /* >>out */
	unsigned char   b_is_debug_chip;                     /* >>out */
	unsigned char   b_is_exclusive_dmem_available;       /* >>out */
	unsigned char   b_is_status_signing_supported;       /* >>out */
	unsigned char   b_is_stream_val_supported;           /* >>out */
	unsigned char   b_is_ksv_list_val_supported;         /* >>out */
	unsigned char   b_is_pre_compute_supported;          /* >>out */
	unsigned int  ret_code;                              /* >>out */
};
#define HDCP_READ_CAPS_ERROR_NONE                  HDCP_ERROR_NONE
#define HDCP_READ_CAPS_CURRENT_BUILD_MODE_PROD     (0x00)
#define HDCP_READ_CAPS_CURRENT_BUILD_MODE_DEBUG_1  (0x01)
#define HDCP_READ_CAPS_CURRENT_BUILD_MODE_DEBUG_2  (0x02)
#define HDCP_READ_CAPS_CURRENT_BUILD_MODE_DEBUG_3  (0x03)
#define HDCP_READ_CAPS_EXCL_DMEM_AVAILABLE         (0x01)
#define HDCP_READ_CAPS_EXCL_DMEM_UNAVAILABLE       (0x00)
#define HDCP_READ_CAPS_STATUS_SIGNING_SUPPORTED    (0x01)
#define HDCP_READ_CAPS_STATUS_SIGNING_UNSUPPORTED  (0x00)
#define HDCP_READ_CAPS_STREAM_VAL_SUPPORTED        (0x01)
#define HDCP_READ_CAPS_STREAM_VAL_UNSUPPORTED      (0x00)
#define HDCP_READ_CAPS_KSVLIST_VAL_SUPPORTED       (0x01)
#define HDCP_READ_CAPS_KSVLIST_VAL_UNSUPPORTED     (0x00)
#define HDCP_READ_CAPS_PRE_COMPUTE_SUPPORTED       (0x01)
#define HDCP_READ_CAPS_PRE_COMPUTE_UNSUPPORTED     (0x00)
#define HDCP_READ_CAPS_DEBUG_CHIP_YES              (0x01)
#define HDCP_READ_CAPS_DEBUG_CHIP_NO               (0x00)
#define HDCP_READ_CAPS_RCV_SUPPORTED               (0x01)
#define HDCP_READ_CAPS_RCV_UNSUPPORTED             (0x00)
#define HDCP_READ_CAPS_CHIP_NAME_T114              "t114"
#define HDCP_READ_CAPS_CHIP_NAME_T148              "t148"
#define HDCP_READ_CAPS_CHIP_NAME_T124              "t124"
#define HDCP_READ_CAPS_CHIP_NAME_T132              "t132"
#define HDCP_READ_CAPS_CHIP_NAME_T210              "t210"
#define HDCP_READ_CAPS_CHIP_NAME_GM107             "gm107"
/*
 * INIT
 *
 * This method will initialize necessary global data needed for HDCP app
 * in TSEC. This includes decrypting LC128, decrypting upstream priv key
 * and setting up sessions pool. If exclusibe DMEM is not available,
 * SET_SCRATCH_BUFFER should precede this method and other methods as
 * documented below. Size of scratch buffer is communicated to client
 * through READ_CAPS method and TSEC HDCP app assumes the SB is allocated
 * to that precise size aligned to 256 bytes. INIT needs to pass the chipId
 * from PMC_BOOT reg.
 *
 * Depends on: [SET_SCRATCH_BUFFER]
 *
 * Error codes:
 *
 * REINIT                  - HDCP application already initialized
 * SB_NOT_SET              - Scratch buffer offset not set
 * INVALID_KEYS            - Decrypting the confidential data failed
 * UNKNOWN                 - Unknown errors while Initing.
 *
 * Flags:
 *
 * FORCE_INIT - Force initialization even if already initialized. Will reset
 *              all the sessions
 */
struct hdcp_init_param {
	unsigned int  flags;                               /* <<in */
	unsigned int  chip_id;                             /* <<in */
	unsigned int  ret_code;                            /* >>out */
};
#define HDCP_INIT_ERROR_NONE                 HDCP_ERROR_NONE
#define HDCP_INIT_ERROR_REINIT               (0x00000001)
#define HDCP_INIT_ERROR_SB_NOT_SET           HDCP_ERROR_SB_NOT_SET
#define HDCP_INIT_ERROR_INVALID_KEYS         (0x00000003)
#define HDCP_INIT_ERROR_UNKNOWN              (0x00000004)
#define HDCP_INIT_FLAG_FORCE_INIT_DISABLE    0
#define HDCP_INIT_FLAG_FORCE_INIT_ENABLE     1
/*
 * CREATE_SESSION
 *
 * A session is synonymous to a secure channel created between the transmitter
 * and receiver. Session keeps track of progress in establishing secure channel
 * by storing all intermediate states. Number of simultaneous sessions will
 * equal the number of wireless displays we plan to support. This method will
 * fail if client tries to create more sessions than supported. Number of
 * sessions is limited only due to the scratch buffer/DMEM constraint.
 * Session is created for version 2.0 by default. Use Update session to
 * change the version. While creating a session, the client needs to pass
 * the expected number of streams used by the receiver. The number of streams
 * cannot be greater than maxStreamsPerReceiver in READ_CAPS method.
 *
 * Depends on: [SET_SCRATCH_BUFFER]
 *
 * Error codes:
 *
 * MAX                  - No free sessions available.
 * SB_NOT_SET           - Scratch buffer is not set.
 * NOT_INIT             - HDCP app not initialized yet.
 * MAX_STREAMS          - noOfStreams is greater than max supported.
 */
struct hdcp_create_session_param {
	unsigned int  no_of_streams;                       /* <<in */
	unsigned int  session_id;                          /* >>out */
	unsigned long long  rtx;                           /* >>out */
	unsigned int  ret_code;                            /* >>out */
	/* 0 - transmitter, 1 - receiver */
	unsigned char   session_type;		           /* <<in */
	/* 0 - WFD, 1 - HDMI, 2 - DP */
	unsigned char   display_type;		           /* <<in: */
	unsigned char   reserved1[2];
	unsigned long long  rrx;                           /* >>out */
};
#define HDCP_CREATE_SESSION_ERROR_NONE            HDCP_ERROR_NONE
#define HDCP_CREATE_SESSION_ERROR_MAX             (0x00000001)
#define HDCP_CREATE_SESSION_ERROR_SB_NOT_SET      HDCP_ERROR_SB_NOT_SET
#define HDCP_CREATE_SESSION_ERROR_NOT_INIT        HDCP_ERROR_NOT_INIT
#define HDCP_CREATE_SESSION_ERROR_INVALID_STREAMS (0x00000004)
#define HDCP_CREATE_SESSION_TYPE_TMTR             (0x00)
#define HDCP_CREATE_SESSION_TYPE_RCVR             (0x01)
/*
 * VERIFY_CERT_RX
 *
 * Verifies receiver public certificate's signature using DCP's public key. If
 * verification succeeds, all necessary information from the certificate are
 * retained in session before returning back to client. Along with certificate,
 * the client also indicates if the wireless receiver is a repeater.
 *
 * Depends on: [SET_SCRATCH_BUFFER, SET_CERT_RX, SET_DCP_KPUB]
 *
 * Error codes:
 *
 * INVALID_SESSION - Session not found
 * SB_NOT_SET      - Scratch buffer not set
 * NOT_INIT        - HDCP app not initialized yet.
 * INVALID_STAGE   - State machine sequence is not followed
 * INVALID_CERT    - Cert validation failed
 * CERT_NOT_SET    - Certiticate offset not set
 * DCP_KPUB_NOT_SET- Dcp public key not set
 * DCP_KPUB_INVALID- Dcp key provided doesn't follow the standards
 *		   - (eg: Null exponent)
 */
struct hdcp_verify_cert_rx_param {
	unsigned int  session_id;                           /* <<in */
	unsigned char   repeater;                           /* <<in */
	unsigned char   reserved[3];
	unsigned int  ret_code;                             /* >>out */
};
#define HDCP_VERIFY_CERT_RX_ERROR_NONE             HDCP_ERROR_NONE
#define HDCP_VERIFY_CERT_RX_ERROR_INVALID_SESSION  HDCP_ERROR_INVALID_SESSION
#define HDCP_VERIFY_CERT_RX_ERROR_SB_NOT_SET       HDCP_ERROR_SB_NOT_SET
#define HDCP_VERIFY_CERT_RX_ERROR_NOT_INIT         HDCP_ERROR_NOT_INIT
#define HDCP_VERIFY_CERT_RX_ERROR_INVALID_STAGE    HDCP_ERROR_INVALID_STAGE
#define HDCP_VERIFY_CERT_RX_ERROR_INVALID_CERT     (0x00000005)
#define HDCP_VERIFY_CERT_RX_ERROR_CERT_NOT_SET     (0x00000006)
#define HDCP_VERIFY_CERT_RX_ERROR_DCP_KPUB_NOT_SET (0x00000007)
#define HDCP_VERIFY_CERT_RX_ERROR_DCP_KPUB_INVALID (0x00000008)
/*
 * GENERATE_EKM
 *
 * Generates 128 bit random number Km and encrypts it using receiver's public
 * ID to generate 1024 bit Ekpub(Km). Km is confidential and not passed to the
 * client, but is saved in session state.
 *
 * Depends on: [SET_SCRATCH_BUFFER]
 *
 * INVALID_SESSION - Session not found
 * SB_NOT_SET      - Scratch Buffer not set
 * NOT_INIT        - HDCP app not initialized yet.
 * INVALID_STAGE   - State machine sequence is not followed
 * DUP_KM          - Session already has a valid Km. Duplicate request
 * RX_KPUB_NOT_SET - Receiver public key not set
 *
 */
struct hdcp_generate_ekm_param {
	unsigned int  session_id;                            /* <<in */
	unsigned char   reserved1[4];
	unsigned long long  ekm[HDCP_SIZE_E_KM_64];          /* >>out */
	unsigned int  ret_code;                              /* >>out */
	unsigned char   reserved2[4];
};
#define HDCP_GENERATE_EKM_ERROR_NONE                HDCP_ERROR_NONE
#define HDCP_GENERATE_EKM_ERROR_INVALID_SESSION     HDCP_ERROR_INVALID_SESSION
#define HDCP_GENERATE_EKM_ERROR_SB_NOT_SET          HDCP_ERROR_SB_NOT_SET
#define HDCP_GENERATE_EKM_ERROR_NOT_INIT            HDCP_ERROR_NOT_INIT
#define HDCP_GENERATE_EKM_ERROR_INVALID_STAGE       HDCP_ERROR_INVALID_STAGE
#define HDCP_GENERATE_EKM_ERROR_RX_KPUB_NOT_SET     (0x00000005)
/*
 * REVOCATION_CHECK
 *
 * Validates if SRM is valid. If yes, checks if receiver ID is in revocation
 * list. Client is supposed to take care of checking the version of SRM and
 * invoking REVOCATION check if SRM is found to be a newer version. This method
 * is applicable to both HDCP1.X and HDCP 2.0 devices. Incase of HDCP 1.X,
 * TSEC will read the BKSV from the display hardware.
 *
 * Depends on: [SET_SCRATCH_BUFFER, SET_SRM, SET_DCP_KPUB]
 *
 * INVALID_SESSION   - Session not found
 * SB_NOT_SET        - Scratch Buffer not set
 * NOT_INIT          - HDCP app not initialized yet.
 * INVALID_STAGE     - Invalid stage
 * INVALID_SRM_SIZE  - Srm size is not valid
 * SRM_VALD_FAILED   - Srm validation failed
 * RCV_ID_REVOKED    - Receiver ID revoked
 * SRM_NOT_SET       - Srm is not set
 * DCP_KPUB_NOT_SET  - DCP Kpub is not set
 */
struct hdcp_revocation_check_param {
	union {
		unsigned int  session_id;
		unsigned int  ap_index;
	} trans_id;
	unsigned char   is_ver_hdcp2x;              /* <<in */
	unsigned char   reserved[3];
	unsigned int  srm_size;                     /* <<in */
	unsigned int  ret_code;                     /* >>out */
};
#define HDCP_REVOCATION_CHECK_ERROR_NONE                 HDCP_ERROR_NONE
#define HDCP_REVOCATION_CHECK_ERROR_INVALID_SESSION\
	HDCP_ERROR_INVALID_SESSION
#define HDCP_REVOCATION_CHECK_ERROR_SB_NOT_SET           HDCP_ERROR_SB_NOT_SET
#define HDCP_REVOCATION_CHECK_ERROR_NOT_INIT             HDCP_ERROR_NOT_INIT
#define HDCP_REVOCATION_CHECK_ERROR_INVALID_STAGE\
	HDCP_ERROR_INVALID_STAGE
#define HDCP_REVOCATION_CHECK_ERROR_INVALID_SRM_SIZE     (0x00000005)
#define HDCP_REVOCATION_CHECK_ERROR_SRM_VALD_FAILED      (0x00000006)
#define HDCP_REVOCATION_CHECK_ERROR_RCV_ID_REVOKED       (0x00000007)
#define HDCP_REVOCATION_CHECK_ERROR_SRM_NOT_SET          (0x00000008)
#define HDCP_REVOCATION_CHECK_ERROR_DCP_KPUB_NOT_SET     (0x00000009)
/*
 * VERIFY_HPRIME
 *
 * Computes H and verifies if H == HPRIME
 *
 * Depends on: [SET_SCRATCH_BUFFER]
 *
 * INVALID_SESSION    - Session not found
 * SB_NOT_SET         - Scratch Buffer not set
 * NOT_INIT           - HDCP app not initialized yet.
 * INVALID_STAGE      - State machine sequence is not followed
 * HPRIME_VALD_FAILED - Hprime validation failed
 */
struct hdcp_verify_hprime_param {
	unsigned long long  hprime[HDCP_SIZE_HPRIME_64];  /* <<in */
	unsigned int  session_id;                         /* <<in */
	unsigned int  ret_code;                           /* >>out */
};
#define HDCP_VERIFY_HPRIME_ERROR_NONE                    HDCP_ERROR_NONE
#define HDCP_VERIFY_HPRIME_ERROR_INVALID_SESSION\
	HDCP_ERROR_INVALID_SESSION
#define HDCP_VERIFY_HPRIME_ERROR_SB_NOT_SET              HDCP_ERROR_SB_NOT_SET
#define HDCP_VERIFY_HPRIME_ERROR_NOT_INIT                HDCP_ERROR_NOT_INIT
#define HDCP_VERIFY_HPRIME_ERROR_INVALID_STAGE\
	HDCP_ERROR_INVALID_STAGE
#define HDCP_VERIFY_HPRIME_ERROR_HPRIME_VALD_FAILED      (0x00000005)
/*
 * ENCRYPT_PAIRING_INFO
 *
 * This encrypts Ekh(km),km and m using the secret key and sends back to client
 * for persistent storage. Will be used when the same receiver is discovered
 * later. Uses HMAC to produce a signature to verify the integrity.
 * m = 64 0's appended to rtx and all are in big-endian format
 * EPair = Eaes(rcvId||m||km||Ekh(Km)||SHA256(rcvId||m||km||Ekh(Km)))
 *       = (40 + 128 + 128 + 128 + 256) bits = 85 bytes  round up to 96
 *
 * Depends on: [SET_SCRATCH_BUFFER]
 *
 * INVALID_SESSION - Session not found
 * SB_NOT_SET      - Scratch Buffer not set
 * NOT_INIT        - HDCP app not initialized yet
 * INVALID_KM      - Km is not initialized
 * INVALID_M       - m is not initialized
 */
struct hdcp_encrypt_pairing_info_param {
	unsigned int  session_id;                         /* <<in */
	unsigned char   reserved1[4];
	unsigned long long  ekhkm[HDCP_SIZE_EKH_KM_64];   /* <<in */
	unsigned long long  e_pair[HDCP_SIZE_EPAIR_64];   /* >>out */
	unsigned int  ret_code;                           /* >>out */
	unsigned char   reserved2[4];
};
#define HDCP_ENCRYPT_PAIRING_INFO_ERROR_NONE             HDCP_ERROR_NONE
#define HDCP_ENCRYPT_PAIRING_INFO_ERROR_INVALID_SESSION\
	HDCP_ERROR_INVALID_SESSION
#define HDCP_ENCRYPT_PAIRING_INFO_ERROR_SB_NOT_SET       HDCP_ERROR_SB_NOT_SET
#define HDCP_ENCRYPT_PAIRING_INFO_ERROR_NOT_INIT         HDCP_ERROR_NOT_INIT
#define HDCP_ENCRYPT_PAIRING_INFO_ERROR_INVALID_KM       (0x00000004)
#define HDCP_ENCRYPT_PAIRING_INFO_ERROR_INVALID_M        (0x00000005)
/*
 * UPDATE_SESSION
 *
 * Updates the session parameters which are determined during key exchange
 * and after it.(like displayid-session mapping)
 *
 * Depends on: [SET_SCRATCH_BUFFER]
 *
 * INVALID_SESSION      - Session not found
 * SB_NOT_SET           - Scratch Buffer not set
 * NOT_INIT             - HDCP app not initialized yet
 * INVALID_STAGE        - State machine sequence is not followed
 * DUP_RRX              - RRX already updated. Duplicate request.
 * INVALID_UPD_MASK     - Update mask is incorrect
 * HDCP_VER_UNSUPPORTED - Version is not supported.
 */
struct hdcp_update_session_param {
	unsigned long long  rrx;                           /* <<in */
	unsigned int  session_id;                          /* <<in */
	unsigned int  head;                                /* <<in */
	unsigned int  or_index;                            /* <<in */
	unsigned int  hdcp_ver;                            /* <<in */
	unsigned int  updmask;                             /* <<in */
	unsigned int  b_recv_pre_compute_support;          /* <<in */
	unsigned int  ret_code;                            /* >>out */
	/* 0-tansmitter 1 - receiver */
	unsigned char   session_type;                      /* <<in: */
	unsigned char   reserved[3];
	unsigned long long  rtx;                           /* <<in */
};
#define HDCP_UPDATE_SESSION_ERROR_NONE                   HDCP_ERROR_NONE
#define HDCP_UPDATE_SESSION_ERROR_INVALID_SESSION\
	HDCP_ERROR_INVALID_SESSION
#define HDCP_UPDATE_SESSION_ERROR_SB_NOT_SET             HDCP_ERROR_SB_NOT_SET
#define HDCP_UPDATE_SESSION_ERROR_NOT_INIT               HDCP_ERROR_NOT_INIT
#define HDCP_UPDATE_SESSION_ERROR_INVALID_STAGE\
	HDCP_ERROR_INVALID_STAGE
#define HDCP_UPDATE_SESSION_ERROR_DUP_RRX                (0x00000005)
#define HDCP_UPDATE_SESSION_ERROR_INVALID_UPD_MASK       (0x00000006)
#define HDCP_UPDATE_SESSION_ERROR_HDCP_VER_UNSUPPORTED   (0x00000007)
#define HDCP_UPDATE_SESSION_MASK_HEAD_PRESENT            (0x00000000)
#define HDCP_UPDATE_SESSION_MASK_ORINDEX_PRESENT         (0x00000001)
#define HDCP_UPDATE_SESSION_MASK_RRX_PRESENT             (0x00000002)
#define HDCP_UPDATE_SESSION_MASK_VERSION_PRESENT         (0x00000003)
#define HDCP_UPDATE_SESSION_MASK_PRE_COMPUTE_PRESENT     (0x00000004)
#define HDCP_UPDATE_SESSION_MASK_RTX_PRESENT             (0x00000008)
#define HDCP_UPDATE_SESSION_PRE_COMPUTE_SUPPORTED        (0x01)
#define HDCP_UPDATE_SESSION_PRE_COMPUTE_UNSUPPORTED      (0x00)
/*
 * GENERATE_LC_INIT
 *
 * Generates 64 bit random number rn.
 * Checks for completed authentication as precondition. Completed auth
 * imply non-null rrx, rtx, km.
 *
 * Depends on: [SET_SCRATCH_BUFFER]
 *
 * INVALID_SESSION  - Session not found
 * SB_NOT_SET       - Scratch Buffer not set
 * NOT_INIT         - HDCP app not initialized yet
 * INVALID_STAGE    - State machine sequence is not followed or
 *                    LC has already succeded for this receiver
 */
struct hdcp_generate_lc_init_param {
	unsigned int  session_id;                           /* <<in */
	unsigned char   reserved1[4];
	unsigned long long  rn;                             /* >>out */
	unsigned int  ret_code;                             /* >>out */
	unsigned char   reserved2[4];
};
#define HDCP_GENERATE_LC_INIT_ERROR_NONE                 HDCP_ERROR_NONE
#define HDCP_GENERATE_LC_INIT_ERROR_INVALID_SESSION\
	HDCP_ERROR_INVALID_SESSION
#define HDCP_GENERATE_LC_INIT_ERROR_SB_NOT_SET           HDCP_ERROR_SB_NOT_SET
#define HDCP_GENERATE_LC_INIT_ERROR_NOT_INIT             HDCP_ERROR_NOT_INIT
#define HDCP_GENERATE_LC_INIT_ERROR_INVALID_STAGE\
	HDCP_ERROR_INVALID_STAGE
/*
 * VERIFY_LPRIME
 *
 * Computes L and verifies if L == LPRIME. Incase of HDCP-2.1 receiver
 * with PRE_COMPUTE support only most significant 128 bits of Lprime is used
 * for comparison.
 *
 * Depends on: [SET_SCRATCH_BUFFER]
 *
 * INVALID_SESSION    - Session not found
 * SB_NOT_SET         - Scratch Buffer not set
 * NOT_INIT           - HDCP app not initialized yet
 * INVALID_STAGE      - State machine sequence is not followed or
 *                      RTT_CHALLENGE not used for HDCP2.1 receiver
 *                      with PRE_COMPUTE support
 * LPRIME_VALD_FAILED - Lprime validation failed
 */
struct hdcp_verify_lprime_param {
	unsigned long long  lprime[HDCP_SIZE_LPRIME_64];  /* <<in */
	unsigned int  session_id;                         /* <<in */
	unsigned int  ret_code;                           /* >>out */
};
#define HDCP_VERIFY_LPRIME_ERROR_NONE                HDCP_ERROR_NONE
#define HDCP_VERIFY_LPRIME_ERROR_INVALID_SESSION     HDCP_ERROR_INVALID_SESSION
#define HDCP_VERIFY_LPRIME_ERROR_SB_NOT_SET          HDCP_ERROR_SB_NOT_SET
#define HDCP_VERIFY_LPRIME_ERROR_NOT_INIT            HDCP_ERROR_NOT_INIT
#define HDCP_VERIFY_LPRIME_ERROR_INVALID_STAGE       HDCP_ERROR_INVALID_STAGE
#define HDCP_VERIFY_LPRIME_ERROR_LPRIME_VALD_FAILED  (0x00000005)
/*
 * GENERATE_SKE_INIT
 *
 * Generates 64 bit random number Riv and encrypted 128 bit session key.
 *
 * Depends on: [SET_SCRATCH_BUFFER]
 *
 * INVALID_SESSION  - Session not found
 * SB_NOT_SET       - Scratch Buffer not set
 * NOT_INIT         - HDCP app not initialized yet
 * LC_INIT_NOT_DONE - LC init phase is not completed yet
 */
struct hdcp_generate_ske_init_param {
	unsigned int  session_id;                             /* <<in */
	unsigned char   reserved1[4];
	unsigned long long  eks[HDCP_SIZE_E_KS_64];           /* >>out */
	unsigned long long  riv;                              /* >>out */
	unsigned int  ret_code;                               /* >>out */
	unsigned char   reserved2[4];
};
#define HDCP_GENERATE_SKE_INIT_ERROR_NONE                HDCP_ERROR_NONE
#define HDCP_GENERATE_SKE_INIT_ERROR_INVALID_SESSION\
	HDCP_ERROR_INVALID_SESSION
#define HDCP_GENERATE_SKE_INIT_ERROR_SB_NOT_SET          HDCP_ERROR_SB_NOT_SET
#define HDCP_GENERATE_SKE_INIT_ERROR_NOT_INIT            HDCP_ERROR_NOT_INIT
#define HDCP_GENERATE_SKE_INIT_ERROR_INVALID_STAGE\
	HDCP_ERROR_INVALID_STAGE
#define HDCP_GENERATE_SKE_INIT_ERROR_LC_INIT_NOT_DONE    (0x00000005)
/*
 * VERIFY_VPRIME
 *
 * Computes V and verifies if V == VPRIME.
 * Does revocation check on the receiver ids got from the repeater along
 * with bstatus checks. maxdevs exceeded and maxcascadeexceeded are considered
 * false. Client should have checked for those values even before calling this
 * method. Applies to both HDCP1.x and HDCP2.x spec.revoID will hold revoked
 * received ID if any. Incase of HDCP-2.1 repeater, vprime holds the most
 * significant 128-bits which will be compared against the most significant
 * 128 bits of V. Incase of successful comparison, least-significant 128 bits
 * of V is returned to the client via v128l. Client needs to populate seqNumV,
 * bHasHdcp20Repeater and bHasHdcp1xDevice only if repeater supports HDCP-2.1
 *
 * Depends on: [SET_SCRATCH_BUFFER, SET_SRM, SET_DCP_KPUB, SET_RECEIVER_ID_LIST]
 *
 * INVALID_SESSION     - Session not found
 * SB_NOT_SET          - Scratch Buffer not set
 * NOT_INIT            - HDCP app not initialized yet
 * INVALID_SRM_SIZE    - Srm size not valid
 * VPRIME_VALD_FAILED  - Vprime validation failed
 * INVALID_APINDEX     - Either Head or OR is not valid
 * SRM_VALD_FAILED     - Srm validation failed
 * RCVD_ID_REVOKED     - Found a revoked receiver ID in receiver Id list
 * SRM_NOT_SET         - Srm not set
 * DCP_KPUB_NOT_SET    - DCP public key not set
 * RCVD_ID_LIST_NOT_SET- Receiver ID list not set
 * SEQ_NUM_V_ROLLOVER  - Seq_Num_V rolled over
 */
struct hdcp_verify_vprime_param {
	unsigned long long  vprime[HDCP_SIZE_VPRIME_2X_64];  /* <<in */
	union {
		unsigned int  session_id;
		unsigned int  ap_index;
	} trans_id;
	unsigned int    srm_size;                                   /* <<in */
	unsigned int    bstatus;                                    /* <<in */
	unsigned char   is_ver_hdcp2x;                              /* <<in */
	unsigned char   depth;                                      /* <<in */
	unsigned char   device_count;                               /* <<in */
	unsigned char   has_hdcp2_repeater;                         /* <<in */
	unsigned char   seqnum[HDCP_SIZE_SEQ_NUM_V_8];              /* <<in */
	unsigned char   has_hdcp1_device;                           /* <<in */
	unsigned char   revoID[HDCP_SIZE_RECV_ID_8];                /* >>out */
	unsigned char   reserved1[7];
	unsigned long long  v128l[HDCP_SIZE_VPRIME_2X_64/2];        /* >>out */
	unsigned int    ret_code;                                   /* >>out */
	unsigned char   reserved2[4];
	unsigned short  rxinfo;
};
#define HDCP_VERIFY_VPRIME_ERROR_NONE                    HDCP_ERROR_NONE
#define HDCP_VERIFY_VPRIME_ERROR_INVALID_SESSION\
	HDCP_ERROR_INVALID_SESSION
#define HDCP_VERIFY_VPRIME_ERROR_SB_NOT_SET              HDCP_ERROR_SB_NOT_SET
#define HDCP_VERIFY_VPRIME_ERROR_NOT_INIT                HDCP_ERROR_NOT_INIT
#define HDCP_VERIFY_VPRIME_ERROR_INVALID_SRM_SIZE        (0x00000004)
#define HDCP_VERIFY_VPRIME_ERROR_VPRIME_VALD_FAILED      (0x00000005)
#define HDCP_VERIFY_VPRIME_ERROR_INVALID_APINDEX         (0x00000006)
#define HDCP_VERIFY_VPRIME_ERROR_SRM_VALD_FAILED         (0x00000007)
#define HDCP_VERIFY_VPRIME_ERROR_RCVD_ID_REVOKED         (0x00000008)
#define HDCP_VERIFY_VPRIME_ERROR_SRM_NOT_SET             (0x00000009)
#define HDCP_VERIFY_VPRIME_ERROR_DCP_KPUB_NOT_SET        (0x0000000A)
#define HDCP_VERIFY_VPRIME_ERROR_RCVD_ID_LIST_NOT_SET    (0x0000000B)
#define HDCP_VERIFY_VPRIME_ERROR_SEQ_NUM_V_ROLLOVER      (0x0000000C)
#define HDCP_VERIFY_VPRIME_ERROR_ATTEMPT_MAX             (0x0000000D)
/*
 * ENCRYPTION_RUN_CTRL
 *
 * To start/stop/pause the encryption for a particular session
 * Incase of HDCP1.X version, apIndex will be used to stop encryption.
 *
 * Depends on: [SET_SCRATCH_BUFFER]
 *
 * INVALID_SESSION  - Session not found
 * SB_NOT_SET       - Scratch Buffer not set
 * NOT_INIT         - HDCP app not initialized yet
 * INVALID_STATE    - Invalid control state
 * INVALID_APINDEX  - Either Head or OR is not valid
 * INVALID_FLAG     - Control flag is invalid
 */
struct hdcp_encryption_run_ctrl_param	{
	union {
		unsigned int  session_id;
		unsigned int  ap_index;
	} trans_id;                                 /* <<in */
	unsigned int  ctrl_flag;                    /* <<in */
	unsigned int  ret_code;                     /* >>out */
};
#define HDCP_ENCRYPTION_RUN_CTRL_ERROR_NONE              HDCP_ERROR_NONE
#define HDCP_ENCRYPTION_RUN_CTRL_ERROR_INVALID_SESSION\
	HDCP_ERROR_INVALID_SESSION
#define HDCP_ENCRYPTION_RUN_CTRL_ERROR_SB_NOT_SET        HDCP_ERROR_SB_NOT_SET
#define HDCP_ENCRYPTION_RUN_CTRL_ERROR_NOT_INIT          HDCP_ERROR_NOT_INIT
#define HDCP_ENCRYPTION_RUN_CTRL_ERROR_INVALID_STAGE\
	HDCP_ERROR_INVALID_STAGE
#define HDCP_ENCRYPTION_RUN_CTRL_ERROR_INVALID_APINDEX   (0x00000005)
#define HDCP_ENCRYPTION_RUN_CTRL_ERROR_INVALID_FLAG      (0x00000006)
#define HDCP_ENCRYPTING_RUN_CTRL_FLAG_START              (0x00000001)
#define HDCP_ENCRYPTING_RUN_CTRL_FLAG_STOP               (0x00000002)
#define HDCP_ENCRYPTING_RUN_CTRL_FLAG_PAUSE              (0x00000003)
/*
 * SESSION_CTRL
 *
 * To activate/reset/delete a session. If deleted, Any future references
 * to this session becomes illegal and this sessionID will be deleted.
 * At any time there may be multiple sessions authenticated, but only
 * few sessions should be activated at a time. This method will help
 * the clients choose which session should be activated/deactivated.
 *
 * Depends on: [SET_SCRATCH_BUFFER]
 *
 * INVALID_SESSION    - Session not found
 * SB_NOT_SET         - Scratch Buffer not set
 * NOT_INIT           - HDCP app not initialized yet
 * INVALID_STAGE      - Activation failed because session is not in proper
 *                    - stage or revocation check is not done.
 * SESSION_ACTIVE     - Error trying to reset or delete an active session.
 *                      This error will also be flagged if client tries to
 *                      activate already active session.
 * SESSION_NOT_ACTIVE - Deactivating a non-active session
 * SESSION_ACTIVE_MAX - Cannot activate a session. Maximum already active.
 */
struct hdcp_session_ctrl_param {
	unsigned int  session_id;                           /* <<in */
	unsigned int  ctrl_flag;                            /* <<in */
	unsigned int  ret_code;                             /* >>out */
};
#define HDCP_SESSION_CTRL_ERROR_NONE                 HDCP_ERROR_NONE
#define HDCP_SESSION_CTRL_ERROR_INVALID_SESSION      HDCP_ERROR_INVALID_SESSION
#define HDCP_SESSION_CTRL_ERROR_SB_NOT_SET           HDCP_ERROR_SB_NOT_SET
#define HDCP_SESSION_CTRL_ERROR_NOT_INIT             HDCP_ERROR_NOT_INIT
#define HDCP_SESSION_CTRL_ERROR_INVALID_STAGE        HDCP_ERROR_INVALID_STAGE
#define HDCP_SESSION_CTRL_ERROR_SESSION_ACTIVE       (0x00000005)
#define HDCP_SESSION_CTRL_ERROR_SESSION_NOT_ACTIVE   (0x00000006)
#define HDCP_SESSION_CTRL_ERROR_SESSION_ACTIVE_MAX   (0x00000007)
#define HDCP_SESSION_CTRL_FLAG_DELETE                (0x00000001)
#define HDCP_SESSION_CTRL_FLAG_ACTIVATE              (0x00000002)
#define HDCP_SESSION_CTRL_FLAG_DEACTIVATE            (0x00000003)
/*
 * VALIDATE_SRM
 *
 * Verifies SRM signature using DCP's public key.
 *
 * Depends on: [SET_SCRATCH_BUFFER, SET_SRM, SET_DCP_KPUB]
 *
 * INVALID_SRM_SIZE  - Srm size invalid
 * SB_NOT_SET        - Scratch Buffer not set
 * NOT_INIT          - HDCP app not initialized yet
 * SRM_VALD_FAILED   - Srm validation failed
 * SRM_NOT_SET       - Srm not set
 * DCP_KPUB_NOT_SET  - DCP public key not set
 */
struct hdcp_validate_srm_param	{
	unsigned int  srm_size;                             /* <<in */
	unsigned int  ret_code;                             /* >>out */
};
#define HDCP_VALIDATE_SRM_ERROR_NONE                     HDCP_ERROR_NONE
#define HDCP_VALIDATE_SRM_ERROR_INVALID_SRM_SIZE         (0x00000001)
#define HDCP_VALIDATE_SRM_ERROR_SB_NOT_SET               HDCP_ERROR_SB_NOT_SET
#define HDCP_VALIDATE_SRM_ERROR_NOT_INIT                 HDCP_ERROR_NOT_INIT
#define HDCP_VALIDATE_SRM_ERROR_SRM_VALD_FAILED          (0x00000004)
#define HDCP_VALIDATE_SRM_ERROR_SRM_NOT_SET              (0x00000005)
#define HDCP_VALIDATE_SRM_ERROR_DCP_KPUB_NOT_SET         (0x00000006)
/*
 * EXCHANGE_INFO
 *
 * To exchange the information between tsechdcp library and TSEC for
 * a particular session.
 *
 * Depends on: [none]
 *
 * INVALID_SESSION - Session not found
 * SB_NOT_SET - Scratch Buffer not set
 * NOT_INIT - HDCP app not initialized yet
 * MSG_UNSUPPORTED - Message applies only to HDCP2.2 receiver
 * INVALID_FLAG - Method flag is invalid
 *
 */
struct hdcp_exchange_info_param {
	unsigned int session_id;            /* <<in */
	unsigned int method_flag;           /* <<in */
	unsigned int ret_code;              /* >>out */
	union {
		struct get_tx_info {
			unsigned char  version;        /* >>out */
			unsigned char  reserved;
			unsigned short tmtr_caps_mask; /* >>out */
		} get_tx_info;
		struct set_rx_info {
			unsigned char  version;        /* >>out */
			unsigned char  reserved;
			unsigned short rcvr_caps_mask; /* <<in */
		} set_rx_info;
		struct set_tx_info {
			unsigned char  version;        /* >>out */
			unsigned char  reserved;
			unsigned short tmtr_caps_mask; /* <<in */
		} set_tx_info;
		struct get_rx_info {
			unsigned char  version;        /* >>out */
			unsigned char  reserved;
			unsigned short rcvr_caps_mask; /* >>out */
		} get_rx_info;
	} info;
};
#define HDCP_EXCHANGE_INFO_GET_TMTR_INFO (0x00000001)
#define HDCP_EXCHANGE_INFO_SET_RCVR_INFO (0x00000002)
#define HDCP_EXCHANGE_INFO_SET_TMTR_INFO (0x00000003)
#define HDCP_EXCHANGE_INFO_GET_RCVR_INFO (0x00000004)
#define HDCP_EXCHANGE_INFO_ERROR_NONE          HDCP_ERROR_NONE
#define HDCP_EXCHANGE_INFO_INVALID_SESSION     HDCP_ERROR_INVALID_SESSION
#define HDCP_EXCHANGE_INFO_ERROR_SB_NOT_SET    HDCP_ERROR_SB_NOT_SET
#define HDCP_EXCHANGE_INFO_ERROR_NOT_INIT      HDCP_ERROR_NOT_INIT
#define HDCP_EXCHANGE_INFO_INVALID_METHOD_FLAG (0x00000006)
/*
 * VALIDATE_DP_STREAM
 *
 * Validates the dp1.2 stream (lprime validation). Only applies to HDCP 1.x.
 * Assumes Vprime is already validated.
 *
 * Depends on: [none]
 *
 * LPRIME_VALD_FAILED  - Lprime validation failed
 * INVALID_APINDEX     - Either head or OR is not valid
 *
 * TODO Incomplete
 */
struct hdcp_validate_dp_stream_param {
	unsigned long long  q_id;                           /* <<in */
	unsigned int  ap_index;                             /* <<in */
	unsigned int  lprime[HDCP_SIZE_LPRIME_1X_32];       /* <<in */
	unsigned int  vprime[HDCP_SIZE_VPRIME_1X_32];       /* <<in */
	unsigned char   dp_stream_id;                       /* <<in */
	unsigned char   reserved1[3];
	unsigned int  ret_code;                             /* >>out */
	unsigned char reserved2[4];
};
#define HDCP_VALIDATE_DP_STREAM_ERROR_NONE               HDCP_ERROR_NONE
#define HDCP_VALIDATE_DP_STREAM_ERROR_LPRIME_VALD_FAILED (0x00000001)
#define HDCP_VALIDATE_DP_STREAM_ERROR_INVALID_APINDEX    (0x00000002)
/*
 * ENCRYPT
 *
 * This method will be used after successfully activating an authenticated
 * session which encrypts the provided plain content using HDCP2.0
 * standard encryption. Method returns the input and stream counter
 * used for encrypting the first block(16-bytes) in the buffer and the client
 * is supposed to derive those counters for successive blocks. This method
 * expects the input and output buffer to be 256 byte aligned and expects
 * proper padding if size of a block in input buffer is less than 16-bytes.
 *
 * Depends on: [SET_SCRATCH_BUFFER, SET_ENC_INPUT_BUFFER, SET_ENC_OUTPUT_BUFFER]
 *
 * INVALID_SESSION      - Session not found
 * SB_NOT_SET           - Scratch Buffer not set
 * NOT_INIT             - HDCP app not initialized yet
 * INVALID_STAGE        - State machine sequence is not followed
 * SESSION_NOT_ACTIVE   - Session is not active for encryption
 * INPUT_BUFFER_NOT_SET - Input buffer not set
 * OUPUT_BUFFER_NOT_SET - Output buffer not set
 * INVALID_STREAM       - Stream ID passed is invalid
 * INVALID_ALIGN        - Either INPUT,OUTPUT or encOffset is not in expected
 *                        alignment
 */
struct hdcp_encrypt_param {
	unsigned int session_id;                           /* <<in */
	unsigned int no_of_input_blocks;                   /* <<in */
	unsigned int stream_id;                            /* <<in */
	unsigned int enc_offset;                           /* <<in */
	unsigned int stream_ctr;                            /* >>out */
	unsigned char  reserved1[4];
	unsigned long long input_ctr;                       /* >>out */
	unsigned long long pes_priv[HDCP_SIZE_PES_HDR_64]; /* >>out */
	unsigned int ret_code;                              /* >>out */
	unsigned char  reserved2[4];
};
#define HDCP_ENCRYPT_ERROR_NONE                   HDCP_ERROR_NONE
#define HDCP_ENCRYPT_ERROR_INVALID_SESSION        HDCP_ERROR_INVALID_SESSION
#define HDCP_ENCRYPT_ERROR_SB_NOT_SET             HDCP_ERROR_SB_NOT_SET
#define HDCP_ENCRYPT_ERROR_NOT_INIT               HDCP_ERROR_NOT_INIT
#define HDCP_ENCRYPT_ERROR_INVALID_STAGE          HDCP_ERROR_INVALID_STAGE
#define HDCP_ENCRYPT_ERROR_SESSION_NOT_ACTIVE     (0x00000005)
#define HDCP_ENCRYPT_ERROR_INPUT_BUFFER_NOT_SET   (0x00000006)
#define HDCP_ENCRYPT_ERROR_OUTPUT_BUFFER_NOT_SET  (0x00000007)
#define HDCP_ENCRYPT_ERROR_INVALID_STREAM         (0x00000008)
#define HDCP_ENCRYPT_ERROR_INVALID_ALIGN          (0x00000009)
/*
 * STREAM_MANAGE
 *
 * This method works only for HDCP-2.1 receivers. Helps in Stream_Manage and
 * Stream_Ready. The input contentID is expected to be in BIG-ENDIAN format.
 * The number of contentIDs and strTypes equals the noOfStreams passed while
 * creating the session. The output seqNumM and streamCtr will have valid
 * values only from index 0 to (noOfStreams-1). Rest will be invalid values.
 *
 * Depends on: [SET_SCRATCH_BUFFER]
 *
 *
 * INVALID_SESSION    - Session not found
 * SB_NOT_SET         - Scratch Buffer not set
 * NOT_INIT           - HDCP app not initialized yet
 * INVALID_STAGE      - RTT challenge is requested in wrong stage
 * MSG_UNSUPPORTED    - Message applies only to HDCP2.1 receiver
 * MPRIME_VALD_FAILED - Mprime validation has failed
 * SEQ_NUM_M_ROLLOVER - Sequence number M has rolled over.
 *
 * FLAGS
 *
 * MANAGE  - Setting this flag will let TSEC return the stream counters
 *           for the video and audio streams associated with session ID.
 *           Only input needed is the sessionID.
 * READY   - Synonymous to STREAM_READY message. Setting this flag will let
 *           TSEC compute M and compare with Mprime. The inputs needed are
 *           contentID, strType (streamType) and Mprime
 */
struct hdcp_stream_manage_param {
	unsigned long long mprime[HDCP_SIZE_MPRIME_64];         /* <<in */
	unsigned int session_id;                                /* <<in */
	unsigned int manage_flag;                               /* <<in */
	unsigned char  content_id[HDCP_MAX_STREAMS_PER_RCVR]
			[HDCP_SIZE_CONTENT_ID_8];               /* <<in */
	unsigned char  str_type[HDCP_MAX_STREAMS_PER_RCVR]
			[HDCP_SIZE_CONTENT_TYPE_8];             /* <<in */
	unsigned char  seqnum_m[HDCP_SIZE_SEQ_NUM_M_8];         /* <<out */
	unsigned char  reserved1[7];
	unsigned int stream_ctr[HDCP_MAX_STREAMS_PER_RCVR];     /* <<out */
	unsigned int ret_code;                                  /* <<out */
	unsigned short streamid_type;
};
#define HDCP_STREAM_MANAGE_ERROR_NONE                    HDCP_ERROR_NONE
#define HDCP_STREAM_MANAGE_ERROR_INVALID_SESSION HDCP_ERROR_INVALID_SESSION
#define HDCP_STREAM_MANAGE_ERROR_SB_NOT_SET              HDCP_ERROR_SB_NOT_SET
#define HDCP_STREAM_MANAGE_ERROR_NOT_INIT                HDCP_ERROR_NOT_INIT
#define HDCP_STREAM_MANAGE_ERROR_INVALID_STAGE HDCP_ERROR_INVALID_STAGE
#define HDCP_STREAM_MANAGE_ERROR_MSG_UNSUPPORTED HDCP_ERROR_MSG_UNSUPPORTED
#define HDCP_STREAM_MANAGE_ERROR_MPRIME_VALD_FAILED      (0x00000006)
#define HDCP_STREAM_MANAGE_ERROR_SEQ_NUM_M_ROLLOVER      (0x00000007)
#define HDCP_STREAM_MANAGE_ERROR_INVALID_FLAG            (0x00000008)
#define HDCP_STREAM_MANAGE_FLAG_MANAGE                   (0x00000001)
#define HDCP_STREAM_MANAGE_FLAG_READY                    (0x00000002)
/*
HDCP Glossary:
---------
HDCP Tsec Application  - An application running in TSEC which will handle HDCP
			 related methods
Client                 - An UMD or KMD component sending methods to HDCP Tsec
			 application to perform any HDCP operation.
Exclusive Dmem         - HDCP Tsec app needs to save persistent states. But
			 since TSEC OS is stateless, we need to wipe-out entire
			 DMEM before context switching to a different
			 application. So we either need to dump all the
			 persistent states to FB or request an exclusive DMEM
			 space that is owned only by HDCP Tsec application and
			 will not be wiped out at all.
Session                - Synonymous to a secure channel between GPU and the
			 wireless display. Number of simultaneous sessioins
			 equals the number of hdcp supported wireless displays
			 discovered.
LC128                  - A global constant provided by DCP LLC to all HDCP
			 adopters.
private key            - RSA private key
public key             - RSA publick key
cert                   - RSA certificate signed by DCP LLC
SRM                    - System renewability message
DCP                    - Digital Content Protection
SB                     - Scratch Buffer
*/
#endif
