#pragma once
#include <cstddef>
#ifdef _WIN32
#  include <windows.h>
#  include <psapi.h>
#else
#  include <unistd.h>
#  include <cstdio>
#endif

namespace cortex {

    inline size_t process_rss_bytes() {
#ifdef _WIN32
        PROCESS_MEMORY_COUNTERS_EX pmc;
        if (GetProcessMemoryInfo(GetCurrentProcess(),
            reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&pmc),
            sizeof(pmc))) {
            return static_cast<size_t>(pmc.WorkingSetSize);
            }
        return 0;
#else
        FILE* f = std::fopen("/proc/self/statm", "r");
        if (!f) return 0;
        long pages=0, resident=0;
        if (std::fscanf(f, "%ld %ld", &pages, &resident) != 2) {
            std::fclose(f);
            return 0;
        }
        std::fclose(f);
        long page_size = sysconf(_SC_PAGESIZE);
        return static_cast<size_t>(resident) * static_cast<size_t>(page_size);
#endif
    }

} // namespace cortex