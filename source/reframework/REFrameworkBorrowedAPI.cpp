#include "REFrameworkBorrowedAPI.hpp"
#include <Windows.h>
#include <ShlObj.h>
#include <fstream>

namespace utility {
    std::string narrow(std::wstring_view str) {
        auto length = WideCharToMultiByte(CP_UTF8, 0, str.data(), (int)str.length(), nullptr, 0, nullptr, nullptr);
        std::string narrowStr{};

        narrowStr.resize(length);
        WideCharToMultiByte(CP_UTF8, 0, str.data(), (int)str.length(), (LPSTR)narrowStr.c_str(), length, nullptr, nullptr);

        return narrowStr;
    }

    std::optional<std::string> get_module_path(HMODULE module) {
        wchar_t filename[MAX_PATH]{0};
        if (GetModuleFileNameW(module, filename, MAX_PATH) >= MAX_PATH) {
            return {};
        }

        return narrow(filename);
    }

    HMODULE get_executable() {
        return GetModuleHandle(nullptr);
    }
}

namespace REFramework {
    static inline bool s_fallback_appdata{false};
    static inline bool s_checked_file_permissions{false};

    std::filesystem::path get_persistent_dir() {
        auto return_appdata_dir = []() -> std::filesystem::path {
            char app_data_path[MAX_PATH]{};
            SHGetSpecialFolderPathA(0, app_data_path, CSIDL_APPDATA, false);
    
            static const auto exe_name = [&]() {
                const auto result = std::filesystem::path(*utility::get_module_path(utility::get_executable())).stem().string();
                const auto dir = std::filesystem::path(app_data_path) / "REFramework" / result;
                std::filesystem::create_directories(dir);
    
                return result;
            }();
    
            return std::filesystem::path(app_data_path) / "REFramework" / exe_name;
        };
    
        if (s_fallback_appdata) {
            return return_appdata_dir();
        }
    
        if (s_checked_file_permissions) {
            static const auto result = std::filesystem::path(*utility::get_module_path(utility::get_executable())).parent_path();
            return result;
        }
    
        // Do some tests on the file creation/writing permissions of the current directory
        // If we can't write to the current directory, we fallback to the appdata folder
        try {
            const auto dir = std::filesystem::path(*utility::get_module_path(utility::get_executable())).parent_path();
            const auto test_file = dir / "test.txt";
            std::ofstream test_stream{test_file};
            test_stream << "test";
            test_stream.close();
    
            std::filesystem::create_directories(dir / "test_dir");
            std::filesystem::remove(test_file);
            std::filesystem::remove(dir / "test_dir");
    
            s_checked_file_permissions = true;
            s_fallback_appdata = false;
        } catch(...) {
            s_fallback_appdata = true;
            s_checked_file_permissions = true;
            return return_appdata_dir();
        }
        
        return std::filesystem::path(*utility::get_module_path(utility::get_executable())).parent_path();
    }
}