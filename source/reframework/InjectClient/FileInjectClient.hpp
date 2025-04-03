#pragma once

#include "../WebPCaptureInjectClient.hpp"

#include <string>

class FileInjectClient : public WebPCaptureInjectClient {
private:
    std::string file_path;
    bool limit_size = false;

public:
    bool provide_webp_data(bool is16x9, ProvideFinishedDataCallback provide_data_finish_callback) override;

    void set_file_path(const std::string& path) {
        file_path = path;
    }

    const std::string& get_file_path() const {
        return file_path;
    }

    void set_limit_size(bool limit) {
        limit_size = limit;
    }

    bool get_limit_size() const {
        return limit_size;
    }
};