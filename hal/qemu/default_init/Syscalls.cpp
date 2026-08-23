#include "hal/qemu/cortex/Semihosting.hpp"
#include "infra/util/ByteRange.hpp"
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cwchar>

extern "C"
{
    int _write(int, const char* buf, int count)
    {
        hal::cortex::SemihostingWrite(infra::ConstByteRange(
            reinterpret_cast<const uint8_t*>(buf),
            reinterpret_cast<const uint8_t*>(buf) + count));
        return count;
    }

    __attribute__((weak)) int swprintf(wchar_t*, size_t, const wchar_t*, ...)
    { 
        return 0; 
    }

    __attribute__((weak)) int vswprintf(wchar_t*, size_t, const wchar_t*, va_list)
    { 
        return 0; 
    }

    __attribute__((weak)) wint_t fgetwc(FILE*)
    { 
        return WEOF; 
    }

    __attribute__((weak)) wint_t getwc(FILE*)
    { 
        return WEOF; 
    }

    __attribute__((weak)) wint_t getwchar()
    { 
        return WEOF; 
    }

    __attribute__((weak)) wint_t ungetwc(wint_t, FILE*)
    { 
        return WEOF; 
    }

    __attribute__((weak)) wint_t fputwc(wchar_t, FILE*)
    { 
        return WEOF; 
    }
    
    __attribute__((weak)) wint_t putwc(wchar_t, FILE*)
    { 
        return WEOF; 
    }
    
    __attribute__((weak)) wint_t putwchar(wchar_t)
    { 
        return WEOF; 
    }
    
    __attribute__((weak)) wchar_t* fgetws(wchar_t*, int, FILE*)
    {
        return nullptr;
    }
    
    __attribute__((weak)) int fputws(const wchar_t*, FILE*)
    { 
        return WEOF; 
    }
}
