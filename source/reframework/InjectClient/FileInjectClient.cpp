#include "FileInjectClient.hpp"
#include "../MHWildsTypes.h"

#include <filesystem>
#include <fstream>

#include <reframework/API.hpp>

bool FileInjectClient::provide_webp_data(bool is16x9, ProvideFinishedDataCallback provide_data_finish_callback) {
    if (file_path.empty() || !is_requested) {
        return false;
    }

    is_requested = false;

    auto& api = reframework::API::get();

    std::filesystem::path path(file_path);
    if (!std::filesystem::exists(path)) {
        api->log_info("File does not exist: %s", file_path.c_str());
        return false;
    }

    if (limit_size) {
        if (std::filesystem::file_size(file_path) > MaxSerializePhotoSizeOriginal) {
            api->log_info("File size exceeds maximum limit: %s of %d bytes", file_path.c_str(), MaxSerializePhotoSizeOriginal);
            return false;
        }
    }

    // Open the file and read the data into a vector
    std::ifstream file(file_path, std::ios::binary);
    if (!file) {
        api->log_info("Failed to open file: %s", file_path.c_str());
        return false;
    }

    std::vector<std::uint8_t> data((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    file.close();

    // Call the callback with the data
    provide_data_finish_callback(true, &data);
    return true;
}
