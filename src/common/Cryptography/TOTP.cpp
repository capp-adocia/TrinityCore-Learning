#include "TOTP.h"
#include <cstring>
#include <openssl/evp.h>
#include <openssl/hmac.h>

constexpr std::size_t Trinity::Crypto::TOTP::RECOMMENDED_SECRET_LENGTH;
static constexpr uint32 TOTP_INTERVAL = 30;
static constexpr uint32 HMAC_RESULT_SIZE = 20;
// 根据密钥和指定时间戳生成token，便于验证
/*static*/ uint32 Trinity::Crypto::TOTP::GenerateToken(Secret const& secret, time_t timestamp)
{
    timestamp /= TOTP_INTERVAL;
    unsigned char challenge[8];
    for (int i = 8; i--; timestamp >>= 8)
        challenge[i] = timestamp;

    unsigned char digest[HMAC_RESULT_SIZE];
    uint32 digestSize = HMAC_RESULT_SIZE;
    // 这里使用SHA1算法，不过现在更推荐使用SHA256，这里应该是为了兼容性
    HMAC(EVP_sha1(), secret.data(), secret.size(), challenge, 8, digest, &digestSize);

    uint32 offset = digest[19] & 0xF; // 取digest的最后一个字节，并取低4位作为偏移量
    uint32 truncated = (digest[offset] << 24) | (digest[offset + 1] << 16) | (digest[offset + 2] << 8) | (digest[offset + 3]); // 根据偏移量取4个字节作为token
    truncated &= 0x7FFFFFFF; // 取低31位，因为token是32位的，最高位是符号位，这里不使用
    return (truncated % 1000000); // 取余数，得到6位token
}
// 验证token是否有效，使用服务器保存的密钥和token进行验证
/*static*/ bool Trinity::Crypto::TOTP::ValidateToken(Secret const& secret, uint32 token)
{
    time_t now = time(nullptr); // 获取当前时间戳
    // 返回的是true或false，表示token是否有效
    return (
        (token == GenerateToken(secret, now - TOTP_INTERVAL)) ||
        (token == GenerateToken(secret, now)) ||
        (token == GenerateToken(secret, now + TOTP_INTERVAL))
    );
}
