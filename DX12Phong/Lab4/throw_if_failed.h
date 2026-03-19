#ifndef THROW_IF_FAILED_
#define THROW_IF_FAILED_

#include <stdexcept>
#include <string>

template<typename T>
inline void ThrowIfFailed(T hr, const char* message = "DirectX operation failed")
{
    if (FAILED(hr))
    {
        throw std::runtime_error(message);
    }
}

#endif //THROWIFFAILED