#pragma once

#define NOMINMAX

#include "../WebPCaptureInjectClient.hpp"
#include "../../reshade/Plugin.h"

#include <reframework/API.hpp>

#include <string>

#include <atomic>
#include <memory>
#include <thread>

#include <Windows.h>

class ReShadeAddOnInjectClient : public WebPCaptureInjectClient {
private:
    ProvideFinishedDataCallback provide_data_finish_callback;

    HMODULE reshade_module = nullptr;

    typedef int (*request_screen_capture_func)(ScreenCaptureFinishFunc finish_callback, int hdr_bit_depths);
    typedef void (*set_reshade_filters_enable_func)(bool should_enable);

    request_screen_capture_func request_reshade_screen_capture = nullptr;
    set_reshade_filters_enable_func set_reshade_filters_enable = nullptr;

    std::unique_ptr<std::thread> webp_compress_thread = nullptr;

    bool is_enabled = true;
    bool should_lossless = false;
    bool use_old_limit_size = false;
    bool is_16x9 = false;
    bool is_photo_mode = false;

    bool request_launched = false;
    bool slowmo_present = false;
    bool slowmo_present_cached = false;

private:
    bool try_load_reshade();
    bool end_slowmo_present();
    void finish_capture(bool success, std::vector<std::uint8_t>* provided_data = nullptr);

    static void compress_webp_thread(std::uint8_t *data, int width, int height);
    static void capture_screenshot_callback(int result, int width, int height, void* data);

    static int pre_action_controller_exec_action(int argc, void** argv, REFrameworkTypeDefinitionHandle* arg_tys, unsigned long long ret_addr);
    static void post_action_controller_exec_action(void** ret_val, REFrameworkTypeDefinitionHandle ret_ty, unsigned long long ret_addr);

    static int pre_open_quest_result_ui(int argc, void** argv, REFrameworkTypeDefinitionHandle* arg_tys, unsigned long long ret_addr);
    static int pre_close_quest_result_ui(int argc, void** argv, REFrameworkTypeDefinitionHandle* arg_tys, unsigned long long ret_addr);
    static void null_post(void** ret_val, REFrameworkTypeDefinitionHandle ret_ty, unsigned long long ret_addr);

public:
    ~ReShadeAddOnInjectClient();

public:
    explicit ReShadeAddOnInjectClient();

    bool provide_webp_data(bool is16x9, ProvideFinishedDataCallback provide_data_finish_callback) override;
    void update();

    void set_enable(bool enable) {
        is_enabled = enable;
    }

    bool get_is_enabled() const {
        return is_enabled;
    }

    void set_lossless(bool lossless) {
        should_lossless = lossless;
    }

    bool is_lossless() const {
        return should_lossless;
    }

    void set_is_photo_mode(bool force) {
        is_photo_mode = force;
    }

    bool get_is_photo_mode() const {
        return is_photo_mode;
    }

    void set_use_old_limit_size(bool use_old_limit) {
        use_old_limit_size = use_old_limit;
    }

    bool get_use_old_limit_size() const {
        return use_old_limit_size;
    }

    bool is_reshade_present() {
        return reshade_module != nullptr && request_reshade_screen_capture != nullptr;
    }

    static ReShadeAddOnInjectClient* get_instance();
    static void initialize();
};