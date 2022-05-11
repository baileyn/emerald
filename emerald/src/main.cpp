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
    auto file_size = fs::file_size(path);
    std::vector<uint8_t> data((std::istreambuf_iterator<char>(file)),
                              std::istreambuf_iterator<char>());

    spdlog::trace(L"Ensuring correct DOS signature");
    auto *dosHeader = reinterpret_cast<PIMAGE_DOS_HEADER>(data.data());
    if (dosHeader->e_magic != IMAGE_DOS_SIGNATURE) {
        spdlog::error(L"Expected injection source to be a DLL. Invalid DOS Header: {:X}", dosHeader->e_magic);
        return false;
    }

    auto *oldNtHeader = reinterpret_cast<PIMAGE_NT_HEADERS>(data.data() + dosHeader->e_lfanew);
    auto oldOptHeader = oldNtHeader->OptionalHeader;
    auto oldFileHeader = oldNtHeader->FileHeader;

    spdlog::trace(L"Checking payload machine to be sure it's 64 bit");
    if (oldFileHeader.Machine != IMAGE_FILE_MACHINE_AMD64) {
        spdlog::error(L"Invalid payload platform. Expected IMAGE_FILE_MACHINE_AMD64({:X}) but got {:X}.",
                      IMAGE_FILE_MACHINE_AMD64, oldFileHeader.Machine);
        return false;
    }

    spdlog::trace(L"Attempting to allocate memory at the preferred base address for payload");
    auto *targetBase = reinterpret_cast<uint8_t*>(VirtualAllocEx(proc,
                                                                 reinterpret_cast<void*>(oldOptHeader.ImageBase),
                                                                 oldOptHeader.SizeOfImage,
                                                                  MEM_COMMIT | MEM_RESERVE,
                                                                 PAGE_EXECUTE_READWRITE));
    if (targetBase == nullptr) {
        spdlog::trace("-- Preferred address unavailable, trying for any address");
        targetBase = reinterpret_cast<uint8_t*>(VirtualAllocEx(proc,
                                                               reinterpret_cast<void*>(oldOptHeader.ImageBase),
                                                               oldOptHeader.SizeOfImage,
                                                                      MEM_COMMIT | MEM_RESERVE,
                                                               PAGE_EXECUTE_READWRITE));

        if (targetBase == nullptr) {
            spdlog::error("Unable to allocate memory to map payload: {:X}", GetLastError());
            return false;
        }
    }

    spdlog::trace(L"Mapping sections to target process");
    auto *sectionHeader = IMAGE_FIRST_SECTION(oldNtHeader);
    for (auto i = 0; i < oldFileHeader.NumberOfSections; i++, sectionHeader++) {
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

    spdlog::trace(L"Freeing payload memory from target process");
    VirtualFreeEx(proc, targetBase, 0, MEM_RELEASE);

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
