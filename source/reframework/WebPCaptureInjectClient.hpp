#pragma once

#include <vector>
#include <functional>

using ProvideFinishedDataCallback = std::function<void(bool, std::vector<std::uint8_t>*)>;

class WebPCaptureInjectClient {
public:
    // Call by the injector to request the client providing the data
    // Return true to affirm you will provide the data, else the injector will do nothing
    virtual bool provide_webp_data(bool is16x9, ProvideFinishedDataCallback provide_data_finish_callback) = 0;
};
