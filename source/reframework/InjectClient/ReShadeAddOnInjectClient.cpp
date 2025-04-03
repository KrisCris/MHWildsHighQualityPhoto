#include "ReShadeAddOnInjectClient.hpp"
#include "../MHWildsTypes.h"
#include "../../reshade/Plugin.h"
#include "../ModSettings.hpp"
#include "../GameUIController.hpp"

#include <reframework/API.hpp>
#include <webp/encode.h>

#include <BS_thread_pool.hpp>

#include <avir.h>
#include <avir_float4_sse.h>

#include <deque>
#include <future>
#include <fstream>

class avir_scale_thread_pool : public avir::CImageResizerThreadPool
{
private:
    BS::thread_pool<> thread_pool;
public:
    virtual int getSuggestedWorkloadCount() const override
    {
        return static_cast<int>(thread_pool.get_thread_count());
    }

    virtual void addWorkload(CWorkload *const workload) override
    {
        _workloads.emplace_back(workload);
    }

    virtual void startAllWorkloads() override
    {
        for (auto &workload : _workloads) _tasks.emplace_back(thread_pool.submit_task([workload](){ workload->process(); }));
    }

    virtual void waitAllWorkloadsToFinish() override
    {
        for (auto &task : _tasks) task.wait();
    }

    virtual void removeAllWorkloads()
    {
        _tasks.clear();
        _workloads.clear();
    }

private:
    std::deque<std::future<void>> _tasks;
    std::deque<CWorkload*> _workloads;
};

std::unique_ptr<ReShadeAddOnInjectClient> reshade_addon_client_instance = nullptr;
std::unique_ptr<avir_scale_thread_pool> avir_thread_pool_instance = nullptr;

static const char *RESHADE_ADDON_NAME = "MHWildsHighQualityPhoto_Reshade.addon";
static const char *END_SLOWMO_PLUGIN_NAME = "end_slowmo.dll";
static const char *GET_SCREEN_CAPTURE_SYMBOL_NAME = "request_screen_capture";
static const char *SET_RESHADE_FILTERS_ENABLE = "set_reshade_filters_enable";

const float QUALITY_REDUCE_STEP = 10.0f;
const float MIN_QUALITY_PHOTO = 10.0f;

const int HIDE_UI_FRAMES_COUNT = 6;
const float START_CAPTURE_AFTER_HIDE_REACHED_PROGRESS = 0.5f;

// NOTE: Change depends on monitor if needed
const int FORCE_SIZE_WIDTH_16x9 = 1920;
const int FORCE_SIZE_HEIGHT_16x9 = 1080;

const int FORCE_SIZE_WIDTH_21x9 = 2560;
const int FORCE_SIZE_HEIGHT_21x9 = 1080;

ReShadeAddOnInjectClient* ReShadeAddOnInjectClient::get_instance() {
    return reshade_addon_client_instance ? reshade_addon_client_instance.get() : nullptr;
}

void ReShadeAddOnInjectClient::initialize() {
    if (reshade_addon_client_instance == nullptr) {
        reshade_addon_client_instance = std::unique_ptr<ReShadeAddOnInjectClient>(new ReShadeAddOnInjectClient());
    }

    if (avir_thread_pool_instance == nullptr) {
        avir_thread_pool_instance = std::make_unique<avir_scale_thread_pool>();
    }
}

bool ReShadeAddOnInjectClient::end_slowmo_present() {
    if (!slowmo_present_cached) {
        slowmo_present_cached = true;
        slowmo_present = false;

        auto &api = reframework::API::get();
        auto slowmo_module = LoadLibraryA(END_SLOWMO_PLUGIN_NAME);

        if (slowmo_module != nullptr) {
            slowmo_present = true;
            api->log_info("Found end slowmo plugin");

            FreeLibrary(slowmo_module);
        } else {
            api->log_info("End slowmo plugin not found, quest result image may not be correct");
        }
    }
    return false;
}

bool ReShadeAddOnInjectClient::provide_webp_data(bool is16x9, ProvideFinishedDataCallback provide_data_finish_callback) {
    if (!is_enabled) {
        reframework::API::get()->log_info("ReShadeAddOnInjectClient is not enabled.");
        return false;
    }

    if (provide_data_finish_callback == nullptr) {
        return false;
    }

    auto game_ui_controller = GameUIController::get_instance();
    if (game_ui_controller == nullptr) {
        reframework::API::get()->log_info("GameUIController instance is null.");
        return false;
    }

    this->provide_data_finish_callback = provide_data_finish_callback;
    this->is_16x9 = is16x9;
    this->request_launched = false;

    game_ui_controller->hide_for(HIDE_UI_FRAMES_COUNT);
    return true;
}

int ReShadeAddOnInjectClient::pre_action_controller_exec_action(int argc, void** argv, REFrameworkTypeDefinitionHandle* arg_tys, unsigned long long ret_addr) {
    if (!reshade_addon_client_instance->end_slowmo_present()) {
        auto game_ui_controller = GameUIController::get_instance();

        if (!reshade_addon_client_instance->request_launched && game_ui_controller->is_in_hide()) {
            return REFRAMEWORK_HOOK_SKIP_ORIGINAL;
        }
    }

    return REFRAMEWORK_HOOK_CALL_ORIGINAL;
}

void ReShadeAddOnInjectClient::post_action_controller_exec_action(void** ret_val, REFrameworkTypeDefinitionHandle ret_ty, unsigned long long ret_addr) {

}

void ReShadeAddOnInjectClient::update() {
    if (!is_enabled) {
        return;
    }

    auto& api = reframework::API::get();
    auto game_ui_controller = GameUIController::get_instance();
    auto mod_settings = ModSettings::get_instance();

    if (game_ui_controller == nullptr || mod_settings == nullptr) {
        return;
    }

    if (!request_launched) {
        if (game_ui_controller->get_hiding_progress() >= START_CAPTURE_AFTER_HIDE_REACHED_PROGRESS) {
            if (is_reshade_present()) {
                auto request_capture = request_reshade_screen_capture(capture_screenshot_callback, mod_settings->hdr_bits);
                if (request_capture != RESULT_SCREEN_CAPTURE_SUBMITTED) {
                    api->log_error("Request capture failed %d", request_capture);
                    finish_capture(false);
                }
                else {
                    api->log_info("Request capture submitted successfully");
                }
            } else {
                api->log_error("Failed to load reshade module");
                finish_capture(false);
            }

            request_launched = true;
        }
    }
}

bool ReShadeAddOnInjectClient::try_load_reshade() {
    if (reshade_module != nullptr) {
        return true;
    }

    if (reshade_module == nullptr) {
        reshade_module = LoadLibraryA(RESHADE_ADDON_NAME);
        if (reshade_module == nullptr) {
            return false;
        }
    }

    if (request_reshade_screen_capture == nullptr) {
        request_reshade_screen_capture = reinterpret_cast<request_screen_capture_func>(GetProcAddress(reshade_module, GET_SCREEN_CAPTURE_SYMBOL_NAME));
    }

    if (set_reshade_filters_enable == nullptr) {
        set_reshade_filters_enable = reinterpret_cast<set_reshade_filters_enable_func>(GetProcAddress(reshade_module, SET_RESHADE_FILTERS_ENABLE));
    }

    return request_reshade_screen_capture != nullptr;
}


void ReShadeAddOnInjectClient::compress_webp_thread(std::uint8_t *data, int width, int height) {
    auto& api = reframework::API::get();

    if (data == nullptr) {
        api->log_info("Data is null");
        reshade_addon_client_instance->finish_capture(false);

        return;
    }

    std::vector<std::uint8_t> new_buffer_if_have;
    bool need_delete_data = true;

    int force_size_width = FORCE_SIZE_WIDTH_16x9;
    int force_size_height = FORCE_SIZE_HEIGHT_16x9;

    if (!reshade_addon_client_instance->is_photo_mode) {
        if (!reshade_addon_client_instance->is_16x9) {
            force_size_width = FORCE_SIZE_WIDTH_21x9;
            force_size_height = FORCE_SIZE_HEIGHT_21x9;
        }
    }

    if (force_size_width != width || force_size_height != height) {
        // Need resize
        api->log_info("Resizing image from %dx%d to %dx%d (GAME REQUIRES IT)", width, height, force_size_width, force_size_height);

        new_buffer_if_have.resize(force_size_width * force_size_height * 4);
        avir::CImageResizer<avir::fpclass_float4> image_resizer( 8 );

        avir::CImageResizerVars params;
        std::memset(&params, 0, sizeof(params));

        params.ThreadPool = avir_thread_pool_instance.get();

        image_resizer.resizeImage(data, width, height, 0, new_buffer_if_have.data(), force_size_width,
            force_size_height, 4, 0, &params);

        data = new_buffer_if_have.data();
        width = force_size_width;
        height = force_size_height;

        need_delete_data = false;
    }
    
    bool is_lossless = reshade_addon_client_instance->is_lossless();

    std::uint8_t* result_temp;
    std::size_t result_size = 0;

    std::size_t max_size = reshade_addon_client_instance->use_old_limit_size ? MaxSerializePhotoSizeOriginal : MaxSerializePhotoSize;
    
    if (is_lossless) {
        result_size = WebPEncodeLosslessRGBA(data, width, height, width * 4, &result_temp);
    } else {
        float min_quality = MIN_QUALITY_PHOTO;
        float current_quality = std::max<float>(MIN_QUALITY_PHOTO, static_cast<float>(ModSettings::get_instance()->max_album_image_quality));

        do {
            result_size = WebPEncodeRGBA(data, width, height, width * 4, current_quality, &result_temp);
            if (result_size == 0) {
                api->log_info("Failed to encode image data to WebP format.");
                break;
            }

            if (result_size >= max_size) {
                current_quality -= QUALITY_REDUCE_STEP;
                api->log_info("Image size too large, reducing quality to %f", current_quality);

                result_size = 0;

                WebPFree(result_temp);
                result_temp = nullptr;
            } else {
                break;
            }
        } while (current_quality >= min_quality);
    }

    if (need_delete_data) {
        delete[] data;
    }

    if (result_size > 0) {
        std::vector<std::uint8_t> temp_buffer(result_temp, result_temp + result_size);
        api->log_info("Screenshot image encoded successfully, size: %zu bytes", result_size);

        reshade_addon_client_instance->finish_capture(true, &temp_buffer);

        WebPFree(result_temp);
    } else {
        // Handle error
        api->log_info("Failed to encode image data to WebP format.");
        reshade_addon_client_instance->finish_capture(false);
    }
}

void ReShadeAddOnInjectClient::capture_screenshot_callback(int result, int width, int height, void* data) {
    auto& api = reframework::API::get();

    if (result == RESULT_SCREEN_CAPTURE_SUCCESS) {
        if (data == nullptr) {
            api->log_info("ReShade's screenshot data is null");
            reshade_addon_client_instance->finish_capture(false);

            return;
        }

        std::uint8_t* data_copy = new std::uint8_t[width * height * 4];
        std::memcpy(data_copy, data, width * height * 4);

        if (reshade_addon_client_instance->webp_compress_thread != nullptr) {
            reshade_addon_client_instance->webp_compress_thread->join();
            reshade_addon_client_instance->webp_compress_thread.reset();
        }

        reshade_addon_client_instance->webp_compress_thread = std::make_unique<std::thread>(compress_webp_thread, data_copy, width, height);
    } else {
        // Handle error
        api->log_info("Screen capture failed with error code: %d", result);
        reshade_addon_client_instance->finish_capture(false);
    }
}

void ReShadeAddOnInjectClient::finish_capture(bool success, std::vector<std::uint8_t>* provided_data) {
    auto& api = reframework::API::get();

    if (success && provided_data) {
        provide_data_finish_callback(success, provided_data);
    } else {
        provide_data_finish_callback(false, nullptr);
    }
}

void ReShadeAddOnInjectClient::null_post(void** ret_val, REFrameworkTypeDefinitionHandle ret_ty, unsigned long long ret_addr) {
    // No operation
}

int ReShadeAddOnInjectClient::pre_open_quest_result_ui(int argc, void** argv, REFrameworkTypeDefinitionHandle* arg_tys, unsigned long long ret_addr) {
    if (!reshade_addon_client_instance->is_enabled) {
        return REFRAMEWORK_HOOK_CALL_ORIGINAL;
    }

    reshade_addon_client_instance->set_reshade_filters_enable(false);

    auto &api = reframework::API::get();
    if (api) {
        api->log_info("Quest result UI opened, disabling ReShade filters to show quest result background.");
    }

    return REFRAMEWORK_HOOK_CALL_ORIGINAL;
}

int ReShadeAddOnInjectClient::pre_close_quest_result_ui(int argc, void** argv, REFrameworkTypeDefinitionHandle* arg_tys, unsigned long long ret_addr) {
    if (!reshade_addon_client_instance->is_enabled) {
        return REFRAMEWORK_HOOK_CALL_ORIGINAL;
    }

    reshade_addon_client_instance->set_reshade_filters_enable(true);

    auto &api = reframework::API::get();
    if (api) {
        api->log_info("Quest result UI closed, enabling ReShade filters.");
    }

    return REFRAMEWORK_HOOK_CALL_ORIGINAL;
}

ReShadeAddOnInjectClient::ReShadeAddOnInjectClient() {
    auto &api = reframework::API::get();

    auto action_controller_exec_action_method = api->tdb()->find_method("ace.cActionController", "execAction");
    action_controller_exec_action_method->add_hook(pre_action_controller_exec_action,
        post_action_controller_exec_action, false);

    auto quest_result_start_method = api->tdb()->find_method("app.GUIFlowQuestResult.cContext", "onStartFlow");
    quest_result_start_method->add_hook(pre_open_quest_result_ui, null_post, false);

    auto quest_result_end_method = api->tdb()->find_method("app.GUIFlowQuestResult.cContext", "onEndFlow");
    quest_result_end_method->add_hook(pre_close_quest_result_ui, null_post, false);

    if (!try_load_reshade()) {
        api->log_error("Failed to load ReShade module");
    }
}

ReShadeAddOnInjectClient::~ReShadeAddOnInjectClient() {
    if (reshade_module != nullptr) {
        FreeLibrary(reshade_module);
        reshade_module = nullptr;
    }

    if (webp_compress_thread != nullptr) {
        webp_compress_thread->join();
        webp_compress_thread.reset();
    }
}