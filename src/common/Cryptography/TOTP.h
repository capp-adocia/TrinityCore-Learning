#ifndef TRINITY_TOTP_H
#define TRINITY_TOTP_H

#include "Define.h"
#include <ctime>
#include <vector>


// 这里使用SHA1算法，不过现在更推荐使用SHA256，这里应该是为了兼容性
namespace Trinity::Crypto
{
    struct TC_COMMON_API TOTP
    {
        static constexpr size_t RECOMMENDED_SECRET_LENGTH = 20;
        // std::string 通常以一个特定的字符（如 '\0'）作为终止符。在某些情况下可能导致意外的结果，
        // 在加密算法中，密钥长度和内容必须精确无误 如果将字符串作为密钥传递，可能会遇到额外的处理问题。
        using Secret = std::vector<uint8>; // 这里优于std::string,它不需要终止符

        static uint32 GenerateToken(Secret const& key, time_t timestamp);
        static bool ValidateToken(Secret const& key, uint32 token);
    };
}

#endif
