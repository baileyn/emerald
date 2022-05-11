#ifndef EMERALD_PROCESS_H
#define EMERALD_PROCESS_H

#include <Windows.h>

#include <vector>
#include <string>

namespace winx::process {
    std::vector<uint32_t> for_name(const std::wstring& name);
}

#endif //EMERALD_PROCESS_H
