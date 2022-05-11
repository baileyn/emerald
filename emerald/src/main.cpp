#include <Shlobj.h>

#include <winx/process.h>
#include <spdlog/spdlog.h>
#include <filesystem>
#include <fstream>
#include <vector>

namespace fs = std::filesystem;

std::filesystem::path get_data_dir() {
    wchar_t* szPath;
    SHGetKnownFolderPath(FOLDERID_RoamingAppData, KF_FLAG_CREATE, nullptr, &szPath);
    auto result = fs::path(szPath);
    CoTaskMemFree(szPath);

    return result;
}

using f_LoadLibraryA = HMODULE (WINAPI*)(LPCSTR lpLibFileName);
using f_GetProcAddress = FARPROC (WINAPI*)(HMODULE hModule, LPCSTR lpProcName);
using f_DllEntryPoint = BOOL (WINAPI*)(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpReserved);
using f_MessageBoxA = int (*)(HWND hWnd, LPCSTR lpText, LPCSTR lpCaption, UINT uType);

struct ManualMappingData {
    f_LoadLibraryA loadLibraryA;
    f_GetProcAddress getProcAddress;
    f_MessageBoxA messageBoxA;
    HINSTANCE hinstDLL;
};

DWORD shellCode(ManualMappingData* data) {
    if ((data == nullptr) || (data->getProcAddress == nullptr) || (data->loadLibraryA == nullptr)) {
        return 1;
    }

    auto *base = reinterpret_cast<PBYTE>(data);
    auto *optionalHeader = &reinterpret_cast<PIMAGE_NT_HEADERS>(base + reinterpret_cast<PIMAGE_DOS_HEADER>(data)->e_lfanew)->OptionalHeader;

    auto loadLibraryA = data->loadLibraryA;
    auto getProcAddress = data->getProcAddress;
    auto dllMain = reinterpret_cast<f_DllEntryPoint>(base + optionalHeader->AddressOfEntryPoint);

    auto locationDelta = reinterpret_cast<UINT_PTR>(base - optionalHeader->ImageBase);
    if (locationDelta != 0) {
        if (optionalHeader->DataDirectory[IMAGE_DIRECTORY_ENTRY_BASERELOC].Size < sizeof(IMAGE_BASE_RELOCATION)) {
            spdlog::error(L"Unable to relocate image because base reloc directory is empty.");
            return 1;
        }

        auto *relocData = reinterpret_cast<PIMAGE_BASE_RELOCATION>(base + optionalHeader->DataDirectory[IMAGE_DIRECTORY_ENTRY_BASERELOC].VirtualAddress);
        while (relocData->VirtualAddress != 0U) {
            if (relocData->SizeOfBlock < sizeof(IMAGE_BASE_RELOCATION)) {
                continue;
            }

            auto numberOfEntries = (relocData->SizeOfBlock - sizeof(IMAGE_BASE_RELOCATION)) / sizeof(WORD);
            auto *relativeInfo = reinterpret_cast<PWORD>(relocData + 1);

            for (auto i = 0; i < numberOfEntries; i++, relativeInfo++) {
                if(relativeInfo != nullptr) {
                    auto *patch = reinterpret_cast<UINT_PTR *>(base + relocData->VirtualAddress +
                                                               (*relativeInfo & 0xFFF));
                    *patch += locationDelta;
                }
            }

            relocData = reinterpret_cast<PIMAGE_BASE_RELOCATION>(reinterpret_cast<PBYTE>(relocData) + relocData->SizeOfBlock);
        }
    }

    if (optionalHeader->DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].Size != 0) {
        auto *importDesc = reinterpret_cast<PIMAGE_IMPORT_DESCRIPTOR>(base + optionalHeader->DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress);
        while (importDesc->Characteristics != 0)
        {
            const auto *name = reinterpret_cast<LPCSTR>(base + importDesc->Name);
            auto *originalFirstThunk = reinterpret_cast<PIMAGE_THUNK_DATA>(base + importDesc->OriginalFirstThunk);
            auto *firstThunk = reinterpret_cast<PIMAGE_THUNK_DATA>(base + importDesc->FirstThunk);
            auto *hDll = loadLibraryA(name);

            if (hDll == nullptr) {
                return 1;
            }

            while (originalFirstThunk->u1.AddressOfData != 0U) {
                if ((originalFirstThunk->u1.Ordinal & IMAGE_ORDINAL_FLAG) != 0U) {
                    auto loadedFunction = getProcAddress(hDll, reinterpret_cast<LPCSTR>(originalFirstThunk->u1.Ordinal & 0xFFFF));

                    if (loadedFunction == nullptr) {
                        return 1;
                    }

                    firstThunk->u1.Function = reinterpret_cast<ULONGLONG>(loadedFunction);
                } else {
                    auto *import = reinterpret_cast<PIMAGE_IMPORT_BY_NAME>(base + originalFirstThunk->u1.AddressOfData);
                    auto loadedFunction = getProcAddress(hDll, reinterpret_cast<LPCSTR>(import->Name));

                    if (loadedFunction == nullptr) {
                        return 1;
                    }

                    firstThunk->u1.Function = reinterpret_cast<ULONGLONG>(loadedFunction);
                }

                originalFirstThunk++;
                firstThunk++;
            }

            importDesc++;
        }
    }

    if (optionalHeader->DataDirectory[IMAGE_DIRECTORY_ENTRY_TLS].Size != 0) {
        auto *tls = reinterpret_cast<PIMAGE_TLS_DIRECTORY>(base + optionalHeader->DataDirectory[IMAGE_DIRECTORY_ENTRY_TLS].VirtualAddress);
        auto *callback = reinterpret_cast<PIMAGE_TLS_CALLBACK*>(tls->AddressOfCallBacks);

        for (; callback != nullptr && *callback != nullptr; callback++) {
            (*callback)(base, DLL_PROCESS_ATTACH, nullptr);
        }
    }

    data->hinstDLL = reinterpret_cast<HINSTANCE>(base);
    return dllMain(data->hinstDLL, DLL_PROCESS_ATTACH, nullptr);
}

bool memory_map(HANDLE proc, const fs::path& path) {
    spdlog::trace(L"Mapping the following payload: {}", path.wstring());
    spdlog::trace(L"Ensuring payload exists");
    if (!fs::exists(path)) {
        spdlog::error(L"Injection Source doesn't exist: {}", path.wstring());
        return false;
    }

    spdlog::trace(L"Opening file");
    std::ifstream file(path, std::ios::binary);
    if (file.fail()) {
        spdlog::error(L"Unable to open file: {:X}", file.rdstate());
        return false;
    }

    spdlog::trace(L"Reading binary file");
    std::vector<uint8_t> data((std::istreambuf_iterator<char>(file)),
                              std::istreambuf_iterator<char>());

    spdlog::trace(L"Ensuring correct DOS signature");
    auto *dosHeader = reinterpret_cast<PIMAGE_DOS_HEADER>(data.data());
    if (dosHeader->e_magic != IMAGE_DOS_SIGNATURE) {
        spdlog::error(L"Expected injection source to be a DLL. Invalid DOS Header: {:X}", dosHeader->e_magic);
        return false;
    }

    auto *oldNtHeader = reinterpret_cast<PIMAGE_NT_HEADERS>(data.data() + dosHeader->e_lfanew);
    auto *oldOptHeader = &oldNtHeader->OptionalHeader;
    auto *oldFileHeader = &oldNtHeader->FileHeader;

    spdlog::trace(L"Checking payload machine to be sure it's 64 bit");
    if (oldFileHeader->Machine != IMAGE_FILE_MACHINE_AMD64) {
        spdlog::error(L"Invalid payload platform. Expected IMAGE_FILE_MACHINE_AMD64({:X}) but got {:X}.",
                      IMAGE_FILE_MACHINE_AMD64, oldFileHeader->Machine);
        return false;
    }

    spdlog::trace(L"Attempting to allocate memory at the preferred base address for payload");
    auto *targetBase = reinterpret_cast<uint8_t*>(VirtualAllocEx(proc,
                                                                 reinterpret_cast<void*>(oldOptHeader->ImageBase),
                                                                 oldOptHeader->SizeOfImage,
                                                                  MEM_COMMIT | MEM_RESERVE,
                                                                 PAGE_EXECUTE_READWRITE));
    if (targetBase == nullptr) {
        spdlog::trace("-- Preferred address unavailable, trying for any address");
        targetBase = reinterpret_cast<uint8_t*>(VirtualAllocEx(proc,nullptr,oldOptHeader->SizeOfImage,
                                                                      MEM_COMMIT | MEM_RESERVE,
                                                               PAGE_EXECUTE_READWRITE));

        if (targetBase == nullptr) {
            spdlog::error("Unable to allocate memory to map payload: {:X}", GetLastError());
            return false;
        }
    }

    spdlog::trace(L"Mapping sections to target process");
    auto *sectionHeader = IMAGE_FIRST_SECTION(oldNtHeader);
    for (auto i = 0; i < oldFileHeader->NumberOfSections; i++, sectionHeader++) {
        spdlog::trace(L"SECTION_HEADER[{}] ({:X}) @ {:X}", i, sectionHeader->SizeOfRawData,
                      sectionHeader->VirtualAddress);

        if (WriteProcessMemory(proc, targetBase + sectionHeader->VirtualAddress,
                           data.data() + sectionHeader->PointerToRawData, sectionHeader->SizeOfRawData,
                           nullptr) == 0) {
            spdlog::error("Unable to map section #{}: {:X}", i, GetLastError());
            VirtualFreeEx(proc, targetBase, 0, MEM_RELEASE);
            return false;
        }
    }

    spdlog::trace(L"Copying the DLL headers to target process");

    ManualMappingData manualMappingData{
            .loadLibraryA = LoadLibraryA,
            .getProcAddress = GetProcAddress,
            .messageBoxA = MessageBoxA,
    };

    memcpy(data.data(), &manualMappingData, sizeof(manualMappingData));
    WriteProcessMemory(proc, targetBase, data.data(), oldOptHeader->SizeOfHeaders, nullptr);

    auto *pShellCode = VirtualAllocEx(proc, nullptr, 0x1000, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    if (pShellCode == nullptr) {
        spdlog::error(L"Unable to allocate memory for our library loading code.");
        return false;
    }
    spdlog::trace(L"Allocated memory for shell code @ {:X}", reinterpret_cast<uintptr_t>(pShellCode));

    spdlog::trace(L"Injecting shell code to load dll");
    WriteProcessMemory(proc, pShellCode, reinterpret_cast<PVOID>(shellCode), 0x1000, nullptr);

    spdlog::trace(L"Executing shell code");
    HANDLE thread = CreateRemoteThread(proc, nullptr, 0, reinterpret_cast<LPTHREAD_START_ROUTINE>(pShellCode), targetBase, 0, nullptr);
    if (thread == nullptr) {
        spdlog::error(L"Unable to execute injected shell code.");
        VirtualFreeEx(proc, targetBase, 0, MEM_RELEASE);
        VirtualFreeEx(proc, pShellCode, 0, MEM_RELEASE);
        return false;
    }

    spdlog::trace(L"Waiting for injection to complete");
    WaitForSingleObject(thread, INFINITE);

    VirtualFreeEx(proc, pShellCode, 0, MEM_RELEASE);

    return true;
}

int main(int argc, char** argv)
{
    spdlog::set_level(spdlog::level::trace);

    const auto *target = L"Notepad.exe";
    const auto data_dir = get_data_dir();

    spdlog::trace(L"Target Process: {}", target);
    spdlog::trace(L"User Data Dir: {}", data_dir.wstring());
    auto processes = winx::process::for_name(target);
    if (processes.empty()) {
        spdlog::error(L"Unable to find target process: {}", target);
        return 1;
    }

    if (processes.size() > 1) {
        spdlog::error(L"Too many running processes: {}", processes.size());
        return 1;
    }

    spdlog::info(L"Target Process: {} ({})", target, processes[0]);

    auto payload_path = fs::current_path() / "emeraldint.dll";
    spdlog::trace(L"Emerald Payload: {} [Exists: {}]", payload_path.wstring(), fs::exists(payload_path));

    auto *handle = OpenProcess(PROCESS_VM_READ | PROCESS_VM_WRITE | PROCESS_VM_OPERATION, false, processes[0]);
    if (handle == INVALID_HANDLE_VALUE) {
        spdlog::error("Unable to open handle to target process.");
        return 1;
    }

    memory_map(handle, payload_path);

    return 0;
}
