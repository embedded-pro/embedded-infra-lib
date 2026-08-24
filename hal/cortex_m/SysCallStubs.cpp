#include <cerrno>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cwchar>
#include <sys/stat.h>

extern "C"
{
    // _write is a no-op by default; a target provides a strong definition to route
    // output to semihosting, a Uart, or an RTT channel.
    [[gnu::weak]] int _write(int, const char*, int count)
    {
        return count;
    }

    [[gnu::weak]] int _read(int, char*, int)
    {
        errno = ENOSYS;
        return -1;
    }

    [[gnu::weak]] int _close(int)
    {
        errno = ENOSYS;
        return -1;
    }

    [[gnu::weak]] long _lseek(int, long, int)
    {
        errno = ENOSYS;
        return -1;
    }

    [[gnu::weak]] int _fstat(int, struct stat* status)
    {
        status->st_mode = S_IFCHR;
        return 0;
    }

    [[gnu::weak]] int _isatty(int)
    {
        return 1;
    }

    [[gnu::weak]] int _getpid()
    {
        errno = ENOSYS;
        return -1;
    }

    [[gnu::weak]] int _kill(int, int)
    {
        errno = ENOSYS;
        return -1;
    }

    // Grows the heap from the "end" symbol supplied by the linker script towards the
    // current stack pointer. A failing stub would be preempted over newlib's own
    // implementation and would break every malloc in the image, so a working default
    // is provided instead; targets that forbid a heap override it with a failing one.
    [[gnu::weak]] void* _sbrk(intptr_t increment)
    {
        extern char end;
        static char* heap = &end;

        char* stackPointer = nullptr;
        __asm volatile("mov %0, sp" : "=r"(stackPointer));

        if (increment > 0 && heap + increment > stackPointer)
        {
            errno = ENOMEM;
            return reinterpret_cast<void*>(-1);
        }

        char* previous = heap;
        heap += increment;
        return previous;
    }

    // arm-none-eabi 15+ nano.specs omits the wide-character I/O that libg_nano.a and
    // libstdc++_nano.a reference transitively through strftime and C++ stream initialization.
    [[gnu::weak]] int swprintf(wchar_t*, size_t, const wchar_t*, ...)
    {
        return 0;
    }

    [[gnu::weak]] int vswprintf(wchar_t*, size_t, const wchar_t*, va_list)
    {
        return 0;
    }

    [[gnu::weak]] wint_t fgetwc(FILE*)
    {
        return WEOF;
    }

    [[gnu::weak]] wint_t getwc(FILE*)
    {
        return WEOF;
    }

    [[gnu::weak]] wint_t getwchar()
    {
        return WEOF;
    }

    [[gnu::weak]] wint_t ungetwc(wint_t, FILE*)
    {
        return WEOF;
    }

    [[gnu::weak]] wint_t fputwc(wchar_t, FILE*)
    {
        return WEOF;
    }

    [[gnu::weak]] wint_t putwc(wchar_t, FILE*)
    {
        return WEOF;
    }

    [[gnu::weak]] wint_t putwchar(wchar_t)
    {
        return WEOF;
    }

    [[gnu::weak]] wchar_t* fgetws(wchar_t*, int, FILE*)
    {
        return nullptr;
    }

    [[gnu::weak]] int fputws(const wchar_t*, FILE*)
    {
        return WEOF;
    }
}
