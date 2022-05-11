#include <Shlobj.h>

#include <winx/process.h>
#include <spdlog/spdlog.h>
#include <filesystem>

std::filesystem::path get_data_dir() {
    wchar_t* szPath;
    SHGetKnownFolderPath(FOLDERID_RoamingAppData, KF_FLAG_CREATE, nullptr, &szPath);
    auto result = std::filesystem::path(szPath);
    CoTaskMemFree(szPath);

    return result;
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

    return 0;
}
