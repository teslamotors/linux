#
# Makefile - Intel Keystore Linux driver
# Copyright (c) 2013, Intel Corporation.
#

ccflags-y += -Isecurity/keystore/include -Isecurity/keystore/ecc -Isecurity/keystore/crypto -Isecurity/keystore

ccflags-$(CONFIG_KEYSTORE_DEVICE_SEED_ONLY) += -DKEYSTORE_DISABLE_USER_SEED
ccflags-$(CONFIG_KEYSTORE_USER_SEED_ONLY) += -DKEYSTORE_DISABLE_DEVICE_SEED
ccflags-$(CONFIG_KEYSTORE_TEST_MIGRATION) += -DKEYSTORE_TEST_MIGRATION
ccflags-$(CONFIG_KEYSTORE_HARD_CODED_SEED) += -DKEYSTORE_HARDCODE_SEED -DDEBUG -DDEBUG_TRACE
ccflags-$(CONFIG_KEYSTORE_TESTMODE) += -DKEYSTORE_TESTMODE

#
# Always enable the mechanisum to retrieve SB status
#
obj-y := sb.o

obj-$(CONFIG_KEYSTORE) +=\
	aes_ccm.o\
	aes_gcm.o\
	aes_ctr.o\
	aes_ecb.o\
	aes_siv.o\
	api_kernel.o\
	check_pub_key.o\
	cmdline.o\
	context.o\
	ecc_encrypt.o\
	ecc_signature.o\
	ecc_keys.o\
	files.o\
	globals.o\
	helpers.o\
	hexdump.o\
	ioctls.o\
	keystore_kmod.o\
	mac.o\
	migration.o\
	seed.o\
	rdrand.o\
	parseasn1_ecc_key.o\
	parse_ecc_keys.o\
	parse_rsa_keys.o\
	ecc/ecc.o\
	ecc/ecies.o\
	ecc/kdf_x963.o\
	ecc/utils.o

# Only compile tests if TESTMODE config option set
obj-$(CONFIG_KEYSTORE_TESTMODE) +=\
	testaes.o\
	testcmac.o\
	testhmac.o\
	testsiv.o\
	testsivsteps.o\
	testecc.o
