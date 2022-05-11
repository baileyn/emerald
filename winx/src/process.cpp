//
// Created by nicho on 5/7/2022.
//

#include <TlHelp32.h>
#include <Windows.h>


#include "winx/process.h"

#include <iostream>

namespace winx::process
{
std::vector<uint32_t> for_name(const std::wstring &name)
{
    std::vector<uint32_t> pids{};
    auto *hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);

    if (hSnapshot == INVALID_HANDLE_VALUE)
    {
        return {};
    }

    PROCESSENTRY32W pe32{
        .dwSize = sizeof(PROCESSENTRY32W),
    };

    if (Process32First(hSnapshot, &pe32))
    {
        do
        {
            if (name == pe32.szExeFile)
            {
                pids.push_back(pe32.th32ProcessID);
            }
        } while (Process32Next(hSnapshot, &pe32));
    }

    return pids;
}
} // namespace winx::process
