/*
 * Native Windows cryptographic implementation for ipmitool LANPLUS.
 * Uses CNG/BCrypt instead of OpenSSL.
 */

#include <assert.h>
#include <stdint.h>
#include <string.h>

#include "ipmitool/log.h"
#include "ipmitool/ipmi_constants.h"
#include "lanplus.h"
#include "lanplus_crypt_impl.h"

#ifdef _WIN32
#include <windows.h>
#include <bcrypt.h>

static const wchar_t *
lanplus_bcrypt_hash_algorithm(uint8_t mac)
{
	if ((mac == IPMI_AUTH_RAKP_HMAC_SHA1) ||
	    (mac == IPMI_INTEGRITY_HMAC_SHA1_96)) {
		return BCRYPT_SHA1_ALGORITHM;
	}
	if ((mac == IPMI_AUTH_RAKP_HMAC_MD5) ||
	    (mac == IPMI_INTEGRITY_HMAC_MD5_128)) {
		return BCRYPT_MD5_ALGORITHM;
	}
#ifdef HAVE_CRYPTO_SHA256
	if ((mac == IPMI_AUTH_RAKP_HMAC_SHA256) ||
	    (mac == IPMI_INTEGRITY_HMAC_SHA256_128)) {
		return BCRYPT_SHA256_ALGORITHM;
	}
#endif
	return NULL;
}

int
lanplus_seed_prng(uint32_t bytes)
{
	uint8_t scratch[64];
	uint32_t remaining = bytes;

	while (remaining > 0) {
		ULONG chunk = remaining > sizeof(scratch) ? sizeof(scratch) : remaining;
		if (BCryptGenRandom(NULL, scratch, chunk,
		                    BCRYPT_USE_SYSTEM_PREFERRED_RNG) < 0) {
			SecureZeroMemory(scratch, sizeof(scratch));
			return 1;
		}
		remaining -= chunk;
	}
	SecureZeroMemory(scratch, sizeof(scratch));
	return 0;
}

int
lanplus_rand(uint8_t *buffer, uint32_t num_bytes)
{
	if (!buffer) {
		return 1;
	}
	return BCryptGenRandom(NULL, buffer, num_bytes,
	                       BCRYPT_USE_SYSTEM_PREFERRED_RNG) < 0;
}

uint8_t *
lanplus_HMAC(uint8_t mac,
             const void *key,
             int key_len,
             const uint8_t *d,
             int n,
             uint8_t *md,
             uint32_t *md_len)
{
	BCRYPT_ALG_HANDLE alg = NULL;
	BCRYPT_HASH_HANDLE hash = NULL;
	const wchar_t *algorithm = lanplus_bcrypt_hash_algorithm(mac);
	DWORD hash_len = 0;
	DWORD result_len = 0;
	NTSTATUS status;

	if (!algorithm || !key || key_len < 0 || !d || n < 0 || !md || !md_len) {
		lprintf(LOG_DEBUG, "Invalid argument in lanplus_HMAC");
		return NULL;
	}

	status = BCryptOpenAlgorithmProvider(&alg, algorithm, NULL,
	                                    BCRYPT_ALG_HANDLE_HMAC_FLAG);
	if (status < 0) {
		lprintf(LOG_DEBUG, "BCryptOpenAlgorithmProvider failed in lanplus_HMAC");
		return NULL;
	}

	status = BCryptGetProperty(alg, BCRYPT_HASH_LENGTH, (PUCHAR)&hash_len,
	                           sizeof(hash_len), &result_len, 0);
	if (status < 0 || hash_len > IPMI_MAX_MD_SIZE) {
		BCryptCloseAlgorithmProvider(alg, 0);
		return NULL;
	}

	status = BCryptCreateHash(alg, &hash, NULL, 0, (PUCHAR)key,
	                          (ULONG)key_len, 0);
	if (status >= 0) {
		status = BCryptHashData(hash, (PUCHAR)d, (ULONG)n, 0);
	}
	if (status >= 0) {
		status = BCryptFinishHash(hash, md, hash_len, 0);
	}

	if (hash) {
		BCryptDestroyHash(hash);
	}
	BCryptCloseAlgorithmProvider(alg, 0);

	if (status < 0) {
		memset(md, 0, IPMI_MAX_MD_SIZE);
		*md_len = 0;
		return NULL;
	}

	*md_len = hash_len;
	return md;
}

static void
lanplus_aes_cbc_128(BOOLEAN encrypt,
                    const uint8_t *iv,
                    const uint8_t *key,
                    const uint8_t *input,
                    uint32_t input_length,
                    uint8_t *output,
                    uint32_t *bytes_written)
{
	BCRYPT_ALG_HANDLE alg = NULL;
	BCRYPT_KEY_HANDLE key_handle = NULL;
	uint8_t local_iv[IPMI_CRYPT_AES_CBC_128_BLOCK_SIZE];
	ULONG out_len = 0;
	NTSTATUS status;

	*bytes_written = 0;
	if (!iv || !key || !input || !output || input_length == 0) {
		return;
	}
	assert((input_length % IPMI_CRYPT_AES_CBC_128_BLOCK_SIZE) == 0);

	memcpy(local_iv, iv, sizeof(local_iv));

	status = BCryptOpenAlgorithmProvider(&alg, BCRYPT_AES_ALGORITHM, NULL, 0);
	if (status >= 0) {
		status = BCryptSetProperty(alg, BCRYPT_CHAINING_MODE,
		                           (PUCHAR)BCRYPT_CHAIN_MODE_CBC,
		                           (ULONG)sizeof(BCRYPT_CHAIN_MODE_CBC), 0);
	}
	if (status >= 0) {
		status = BCryptGenerateSymmetricKey(alg, &key_handle, NULL, 0,
		                                    (PUCHAR)key,
		                                    IPMI_CRYPT_AES_CBC_128_BLOCK_SIZE,
		                                    0);
	}
	if (status >= 0) {
		if (encrypt) {
			status = BCryptEncrypt(key_handle, (PUCHAR)input, input_length,
			                       NULL, local_iv, sizeof(local_iv),
			                       output, input_length, &out_len, 0);
		} else {
			status = BCryptDecrypt(key_handle, (PUCHAR)input, input_length,
			                       NULL, local_iv, sizeof(local_iv),
			                       output, input_length, &out_len, 0);
		}
	}

	if (key_handle) {
		BCryptDestroyKey(key_handle);
	}
	if (alg) {
		BCryptCloseAlgorithmProvider(alg, 0);
	}
	SecureZeroMemory(local_iv, sizeof(local_iv));

	if (status >= 0) {
		*bytes_written = out_len;
	}
}

void
lanplus_encrypt_aes_cbc_128(const uint8_t *iv,
                            const uint8_t *key,
                            const uint8_t *input,
                            uint32_t input_length,
                            uint8_t *output,
                            uint32_t *bytes_written)
{
	lanplus_aes_cbc_128(TRUE, iv, key, input, input_length, output,
	                    bytes_written);
}

void
lanplus_decrypt_aes_cbc_128(const uint8_t *iv,
                            const uint8_t *key,
                            const uint8_t *input,
                            uint32_t input_length,
                            uint8_t *output,
                            uint32_t *bytes_written)
{
	lanplus_aes_cbc_128(FALSE, iv, key, input, input_length, output,
	                    bytes_written);
}

#else
#error This Win32 port expects native Windows CNG for LANPLUS cryptography.
#endif
