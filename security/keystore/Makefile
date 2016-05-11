#
# Makefile - Intel Keystore Linux driver
# Copyright (c) 2013, Intel Corporation.
#

ccflags-y += -Isecurity/keystore/include -Isecurity/keystore/crypto

# Keystore module
obj-$(CONFIG_KEYSTORE) := keystore.o

keystore-y :=\
	keystore_kmod.o\
	api_kernel.o\
	api_kernel_migration.o\
	client.o \
	context.o\
	context_safe.o\
	ioctls.o\
	operations.o\
	seed.o

# ECC library
keystore-y +=\
	crypto/ecc/ecc.o\
	crypto/ecc/ecies.o\
	crypto/ecc/kdf_x963.o\
	crypto/ecc/utils.o\
	crypto/ecc/ecc_encrypt.o\
	crypto/ecc/ecc_signature.o\
	crypto/ecc/ecc_keys.o

# Hash functions
keystore-y +=\
	crypto/mac/mac.o

# Random numbers
keystore-y +=\
	crypto/rand/rdrand.o

# AES library
keystore-y +=\
	crypto/aes/aes_utils.o\
	crypto/aes/aes_ccm.o\
	crypto/aes/aes_gcm.o\
	crypto/aes/aes_ctr.o\
	crypto/aes/aes_ecb.o\
	crypto/aes/aes_siv.o

keystore-$(CONFIG_KEYSTORE_DEBUG) +=\
	debug.o

keystore-$(CONFIG_KEYSTORE_TEST_MIGRATION) +=\
	api_kernel_migration_test.o

keystore-$(CONFIG_KEYSTORE_TESTMODE) +=\
	test/test_run.o\
	test/testaes.o\
	test/testcmac.o\
	test/testhmac.o\
	test/testsiv.o\
	test/testsivsteps.o\
	test/testecc.o\
	test/testoperations.o

keystore-$(CONFIG_APPLICATION_AUTH) +=\
        appauth/file_hash_verify.o\
        appauth/key_setup.o\
        appauth/manifest_parser.o\
        appauth/manifest_read.o\
        appauth/manifest_verify.o\
        appauth/sign_verify.o
