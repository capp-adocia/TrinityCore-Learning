#ifndef TRINITY_CRYPTORANDOM_H
#define TRINITY_CRYPTORANDOM_H

#include "Define.h"
#include <array>

namespace Trinity::Crypto
{
    void TC_COMMON_API GetRandomBytes(uint8* buf, size_t len);

    template <typename Container>
    void GetRandomBytes(Container& c)
    {
        GetRandomBytes(std::data(c), std::size(c));
    }

    template <size_t S>
    std::array<uint8, S> GetRandomBytes()
    {
        std::array<uint8, S> arr;
        GetRandomBytes(arr);
        return arr;
    }
}

#endif
