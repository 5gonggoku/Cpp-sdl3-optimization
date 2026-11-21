#pragma once
#include <cstdint>
#include <string>

struct EnvInfo {
    std::string cpu;
    std::uint64_t ramMB = 0;
    std::string os;
    std::string compiler;
    std::string buildType;
    std::string commit;
};

EnvInfo GetEnvInfo();
