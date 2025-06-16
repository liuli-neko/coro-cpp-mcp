#pragma once

#include <iostream>

#if defined(_WIN32)
// clang-format off
#include <windows.h>
#include <psapi.h>
// clang-format on

#pragma comment(lib, "psapi.lib")

#elif defined(__linux__)
#include <fstream>
#include <sstream>
#include <string>

#elif defined(__APPLE__)
#include <mach/mach.h>
#endif

/**
 * @brief 获取当前进程使用的物理内存大小 (Resident Set Size)
 * @return 内存大小，单位为字节。获取失败则返回 0。
 */
size_t getCurrentRSS() {
#if defined(_WIN32)
    PROCESS_MEMORY_COUNTERS pmc;
    if (GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc))) {
        return pmc.WorkingSetSize;
    }
    return 0;

#elif defined(__linux__)
    std::ifstream statusFile("/proc/self/status");
    if (!statusFile.is_open()) return 0;

    std::string line;
    while (std::getline(statusFile, line)) {
        if (line.rfind("VmRSS:", 0) == 0) {
            std::istringstream iss(line);
            std::string key;
            long value;
            iss >> key >> value;
            return value * 1024; // VmRSS is in KB, convert to bytes
        }
    }
    return 0;

#elif defined(__APPLE__)
    mach_task_basic_info_data_t taskInfo;
    mach_msg_type_number_t infoCount = MACH_TASK_BASIC_INFO_COUNT;
    if (task_info(mach_task_self(), MACH_TASK_BASIC_INFO, (task_info_t)&taskInfo, &infoCount) == KERN_SUCCESS) {
        return taskInfo.resident_size;
    }
    return 0;
#else
    // 不支持的平台
    return 0;
#endif
}
struct ResourceUsage {
    std::string memory;
};
struct ToolCallInfo {
    uint64_t executionTime;
    ResourceUsage resourceUsage;
};