#pragma once

#include "../WebPCaptureInjectClient.hpp"

class NullCaptureInjectClient : public WebPCaptureInjectClient {
public:
    bool provide_webp_data(bool is16x9, ProvideFinishedDataCallback provide_data_finish_callback) override {
        return false;
    }
};