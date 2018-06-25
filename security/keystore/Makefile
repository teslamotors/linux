#
# Makefile - Intel Keystore Linux driver
# Copyright (c) 2013, Intel Corporation.
#

ccflags-y += -Isecurity/keystore/include -Isecurity/keystore/crypto
ccflags-$(CONFIG_KEYSTORE_DEVICE_SEED_ONLY) += -DKEYSTORE_DISABLE_USER_SEED
ccflags-$(CONFIG_KEYSTORE_USER_SEED_ONLY) += -DKEYSTORE_DISABLE_DEVICE_SEED
# If we are just wiping seeds, disable both seeds
ccflags-$(CONFIG_KEYSTORE_SEED_WIPE) += -DKEYSTORE_DISABLE_USER_SEED -DKEYSTORE_DISABLE_DEVICE_SEED

# Keystore module
obj-$(CONFIG_KEYSTORE) := keystore.o

# Files common to keystore (full) and the seed wiper
keystore-y :=\
	seed.o

# If we are just wiping the seeds, we only need one file
keystore-$(CONFIG_KEYSTORE_SEED_WIPE) +=\
	seed_wipe.o

# Otherwise, select the full file list
keystore-$(CONFIG_KEYSTORE_FULL) +=\
	keystore_kmod.o\
	api_kernel.o\
	api_kernel_migration.o\
	client.o \
	context.o\
	context_safe.o\
	ioctls.o\
	operations.o

# ECC library
keystore-$(CONFIG_KEYSTORE_FULL) +=\
	crypto/ecc/ecc.o\
	crypto/ecc/ecies.o\
	crypto/ecc/kdf_x963.o\
	crypto/ecc/utils.o\
	crypto/ecc/ecc_encrypt.o\
	crypto/ecc/ecc_signature.o\
	crypto/ecc/ecc_keys.o

# Hash functions
keystore-$(CONFIG_KEYSTORE_FULL) +=\
	crypto/mac/mac.o

# Random numbers
keystore-$(CONFIG_KEYSTORE_FULL) +=\
	crypto/rand/rdrand.o

# AES library
keystore-$(CONFIG_KEYSTORE_FULL) +=\
	crypto/aes/aes_utils.o\
	crypto/aes/aes_aead.o\
	crypto/aes/aes_ctr.o\
	crypto/aes/aes_ecb.o\
	crypto/aes/aes_siv.o

keystore-$(CONFIG_DAL_KEYSTORE) +=\
	dal-client/api_dal.o\
	dal-client/dal_client.o\
	dal-client/applet_read.o\
	dal-client/dal_keystore_conf.o\
	dal-client/dal_context.o\
	dal-client/dal_context_safe.o

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
	appauth/manifest_cache.o\
	appauth/manifest_parser.o\
	appauth/manifest_read.o\
	appauth/manifest_verify.o\
	appauth/sign_verify.o \
	appauth/manifest_sanity_check.o
