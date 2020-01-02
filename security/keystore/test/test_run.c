#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/bug.h>
#include <linux/errno.h>

#include "keystore_tests.h"
#include "keystore_debug.h"

struct keystore_test tests[] = {
	{
		.name = "CMAC",
		.run  = keystore_test_cmac
	},
	{
		.name = "HMAC",
		.run  = keystore_test_hmac
	},
	{
		.name = "AES",
		.run  = keystore_test_aes
	},
	{
		.name = "AES SIV Steps",
		.run  = keystore_test_aes_siv_steps
	},
	{
		.name = "AES SIV Steps with Nonce",
		.run  = keystore_test_aes_siv_nonce
	},
	{
		.name = "AES_SIV",
		.run  = keystore_test_aes_siv
	},
	{
		.name = "ECC",
		.run  = keystore_test_ecc
	},
	{
		.name = "Gen Key AES",
		.run  = keystore_test_genkey_aes
	},
	{
		.name = "Gen Key ECC",
		.run  = keystore_test_genkey_ecc
	},
	{
		.name = "Wrap/Unwrap",
		.run  = keystore_test_wrap_unwrap
	},
	{
		.name = "Encrypt/Decrypt AES-CCM",
		.run  = keystore_test_encrypt_decrypt_algo_aes_ccm
	},
	{
		.name = "Encrypt/Decrypt AES-GCM",
		.run  = keystore_test_encrypt_decrypt_algo_aes_gcm
	},
	{
		.name = "Encrypt/Decrypt ECIES",
		.run  = keystore_test_encrypt_decrypt_algo_ecies
	},
	{
		.name = "Sign/Verify ECDSA",
		.run  = keystore_test_sign_verify_algo_ecdsa
	},
	{
		.name = "Encrypt/Decrypt for host",
		.run = keystore_test_encrypt_for_host
	}
};

int _keystore_assert(bool x, const char *test,
		     const char *func, unsigned int line)
{
	if (x) {
		ks_info(KBUILD_MODNAME ": %s():%u %s Passed.\n",
			func, line, test);
		return 0;
	}

	ks_warn(KBUILD_MODNAME ": %s():%u %s Failed!\n",
		func, line, test);
	return -1;
}

int keystore_run_tests(void)
{
	int res = 0;
	unsigned int i;

	for (i = 0; i < ARRAY_SIZE(tests); ++i)	{
		ks_info(KBUILD_MODNAME ": Running test %u: %s\n",
			i, tests[i].name);
		if (!tests[i].run) {
			ks_info(KBUILD_MODNAME ": Null test pointer\n");
			tests[i].result = -EFAULT;
			continue;
		}
		tests[i].result = tests[i].run();
		ks_info(KBUILD_MODNAME ": Test %u result: %d: ",
			i, tests[i].result);
		if (tests[i].result) {
			res = tests[i].result;
			ks_info("NOK\n");
		} else {
			ks_info("OK!\n");
		}
	}

	return res;
}
