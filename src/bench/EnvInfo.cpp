#include "bench/EnvInfo.h"

#include <cstdio>
#include <cstring>

#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <intrin.h>

#ifndef GAME_GIT_COMMIT
#define GAME_GIT_COMMIT "unknown"
#endif
#ifndef GAME_BUILD_TYPE
#define GAME_BUILD_TYPE "unknown"
#endif

namespace {

std::string ReadCpuBrand()
{
    int regs[4] = {};
    __cpuid(regs, static_cast<int>(0x80000000));
    if (static_cast<unsigned>(regs[0]) < 0x80000004u) return "unknown";

    char brand[49] = {};
    for (unsigned i = 0; i < 3; ++i) {
        __cpuid(regs, static_cast<int>(0x80000002u + i));
        std::memcpy(brand + i * 16, regs, 16);
    }

    const char* p = brand;
    while (*p == ' ') ++p;
    std::string result = p;
    while (!result.empty() && result.back() == ' ') result.pop_back();
    return result;
}

std::string ReadOsVersion()
{
    using RtlGetVersionFn = LONG(WINAPI*)(OSVERSIONINFOW*);
    HMODULE ntdll = GetModuleHandleW(L"ntdll.dll");
    if (!ntdll) return "Windows (unknown)";

    FARPROC proc = GetProcAddress(ntdll, "RtlGetVersion");
    if (!proc) return "Windows (unknown)";
    RtlGetVersionFn fn = nullptr;
    std::memcpy(&fn, &proc, sizeof(fn));

    OSVERSIONINFOW info = {};
    info.dwOSVersionInfoSize = sizeof(info);
    if (fn(&info) != 0) return "Windows (unknown)";

    char buf[64];
    std::snprintf(buf, sizeof(buf), "Windows %lu.%lu.%lu", info.dwMajorVersion, info.dwMinorVersion,
                  info.dwBuildNumber);
    return buf;
}

}

EnvInfo GetEnvInfo()
{
    EnvInfo env;
    env.cpu = ReadCpuBrand();

    MEMORYSTATUSEX mem = {};
    mem.dwLength = sizeof(mem);
    if (GlobalMemoryStatusEx(&mem)) env.ramMB = mem.ullTotalPhys / (1024ull * 1024ull);

    env.os = ReadOsVersion();

    char cc[64];
#if defined(_MSC_FULL_VER)
    std::snprintf(cc, sizeof(cc), "MSVC %d", static_cast<int>(_MSC_FULL_VER));
#else
    std::snprintf(cc, sizeof(cc), "unknown");
#endif
    env.compiler = cc;
    env.buildType = GAME_BUILD_TYPE;
    env.commit = GAME_GIT_COMMIT;
    return env;
}
