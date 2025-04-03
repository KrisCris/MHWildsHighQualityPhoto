// Install DebugView to view the OutputDebugString calls
#define CIMGUI_DEFINE_ENUMS_AND_STRUCTS
#define CIMGUI_NO_EXPORT

#include <sstream>
#include <mutex>

#include <Windows.h>

#include <reframework/API.hpp>
#include <webp/encode.h>
#include <cimgui.h>

#include "MHWildsTypes.h"
#include "../reshade/Plugin.h"

#include <fstream>

using namespace reframework;

typedef void* (*REFGenericFunction)(...);

#undef API

void on_draw_ui(REFImGuiFrameCbData* data) {
    if (igCollapsingHeader_TreeNodeFlags("High Quality Photos", ImGuiTreeNodeFlags_None)) {
        igText("Hello from the super cool plugin!");
    }
}

bool has_custom_screenshot_started = false;
bool is_saved = false;
bool is_capture_for_quest_result = false;

HMODULE reshade_module = nullptr;

std::unique_ptr<std::vector<std::uint8_t>> copied_buffer = nullptr;
std::atomic_bool finished_capture = false;

typedef int (*request_screen_capture_func)(ScreenCaptureFinishFunc finish_callback, int hdr_bit_depths);

request_screen_capture_func request_reshade_screen_capture = nullptr;
std::unique_ptr<std::thread> webp_compress_thread = nullptr;

bool try_load_reshade() {
    if (reshade_module != nullptr) {
        return true;
    }

    if (reshade_module == nullptr) {
        reshade_module = LoadLibraryA("MHWildsHighQualityPhoto_Reshade.addon");
        if (reshade_module == nullptr) {
            return false;
        }
    }

    if (request_reshade_screen_capture == nullptr) {
        request_reshade_screen_capture = (request_screen_capture_func)GetProcAddress(reshade_module, "request_screen_capture");
    }

    return request_reshade_screen_capture != nullptr;
}

int pre_start_update_save_capture(int argc, void** argv, REFrameworkTypeDefinitionHandle* arg_tys, unsigned long long ret_addr) {
    auto obj = API::get()->get_managed_singleton("app.AlbumManager");
    auto capture_state_ptr = obj->get_field<int>("_SaveCaptureState");

    if (has_custom_screenshot_started) {
        if (capture_state_ptr != nullptr) {
            if (*capture_state_ptr == SAVECAPTURESTATE_WAIT_SERIALIZE && !finished_capture) {
                return REFRAMEWORK_HOOK_SKIP_ORIGINAL;
            }
        }
    }

    return REFRAMEWORK_HOOK_CALL_ORIGINAL;
}

void compress_webp_thread(std::uint8_t *data, int width, int height) {
    if (data == nullptr) {
        finished_capture = true;
        return;
    }

    std::uint8_t* result_temp;
    std::size_t result_size = 0;
    
    if (is_capture_for_quest_result) {
        result_size = WebPEncodeLosslessRGBA(data, width, height, width * 4, &result_temp);
    } else {
        float min_quality = 10.0f;
        float current_quality = 100.0f;

        do {
            result_size = WebPEncodeRGBA(data, width, height, width * 4,current_quality, &result_temp);
            if (result_size == 0) {
                API::get()->log_info("Failed to encode image data to WebP format.");
                break;
            }
            API::get()->log_info("Image size packed %d", result_size);
            if (result_size >= MaxSerializePhotoSize) {
                API::get()->log_info("Image size too large, reducing quality to %f", current_quality);
                current_quality -= 10.0f;
                result_size = 0;

                WebPFree(result_temp);
                result_temp = nullptr;
            } else {
                break;
            }
        } while (current_quality >= min_quality);
    }

    delete[] data;

    if (result_size > 0) {
        std::ofstream file("D:\\preview_result.webp", std::ios::binary | std::ios::out | std::ios::trunc);
        file.write((char*)result_temp, result_size);
        file.flush();
        file.close();
        copied_buffer = std::make_unique<std::vector<std::uint8_t>>(result_temp, result_temp + result_size);
        WebPFree(result_temp);
    } else {
        // Handle error
        API::get()->log_info("Failed to encode image data to WebP format.");
    }

    finished_capture = true;
}

void capture_screenshot_callback(int result, int width, int height, void* data) {
    if (result == RESULT_SCREEN_CAPTURE_SUCCESS) {
        if (data == nullptr) {
            API::get()->log_info("Data is null");
            finished_capture = true;
            return;
        }

        std::uint8_t* data_copy = new std::uint8_t[width * height * 4];
        std::memcpy(data_copy, data, width * height * 4);

        if (webp_compress_thread != nullptr) {
            webp_compress_thread->join();
        }

        webp_compress_thread = std::make_unique<std::thread>(compress_webp_thread, data_copy, width, height);
    } else {
        // Handle error
        API::get()->log_info("Screen capture failed with error code: %d", result);
        finished_capture = true;
    }
}

int hide_frame_passed = -1;
int hide_frame_total = 4;

bool is_in_hide_frame() {
    return hide_frame_passed >= 0 && hide_frame_passed < hide_frame_total;
}

bool should_start_capture_after_hide() {
    return hide_frame_passed == (int)(hide_frame_total / 2);
}

void enable_hide_frame() {
    const int GUI_PHOTO_MODE_ID = 100;

    if (is_in_hide_frame()) {
        return;
    }

    hide_frame_passed = 0;

    if (is_capture_for_quest_result) {
        hide_frame_total = 16;
    }
    else {
        hide_frame_total = 16;
    }

    /*
    auto singleton = API::get()->get_managed_singleton("app.GUIManager");
    auto accessor_ptr = *singleton->get_field<API::ManagedObject*>("_AccessorForGUIFlow");
    auto gui = accessor_ptr->call<API::ManagedObject*>("getGUI", API::get()->get_vm_context(), accessor_ptr, GUI_PHOTO_MODE_ID);
    gui->call("requestDisplay", API::get()->get_vm_context(), gui, false, true);*/
}

void increment_hide_frame() {
    if (!is_in_hide_frame()) {
        return;
    }

    const int GUI_PHOTO_MODE_ID = 100;
    hide_frame_passed++;

    if (hide_frame_passed >= hide_frame_total) {
        hide_frame_passed = -1;
            
        /*
        auto singleton = API::get()->get_managed_singleton("app.GUIManager");
        auto accessor_ptr = *singleton->get_field<API::ManagedObject*>("_AccessorForGUIFlow");
        auto gui = accessor_ptr->call<API::ManagedObject*>("getGUI", API::get()->get_vm_context(), accessor_ptr, GUI_PHOTO_MODE_ID);
        gui->call("requestDisplay", API::get()->get_vm_context(), gui, true, true);*/
    }
}

void post_start_update_save_capture(void** ret_val, REFrameworkTypeDefinitionHandle ret_ty, unsigned long long ret_addr) {
    auto obj = API::get()->get_managed_singleton("app.AlbumManager");
    auto capture_state_ptr = obj->get_field<int>("_SaveCaptureState");

    if (capture_state_ptr == nullptr) {
        API::get()->log_info("Capture state null");
        return;
    } else {
        if (*capture_state_ptr == SAVECAPTURESTATE_SERIALIZE && !has_custom_screenshot_started) {
            has_custom_screenshot_started = true;
            finished_capture = false;

            enable_hide_frame();

            is_capture_for_quest_result = false;
            auto capture_mode_ptr = obj->get_field<int>("_CaptureMode");

            if (capture_mode_ptr && *capture_mode_ptr == 1) {
                is_capture_for_quest_result = true;
                API::get()->log_info("Use lossless mode for capture encode");
            }
        }

        if (is_in_hide_frame()) {
            if (should_start_capture_after_hide()) {
                if (try_load_reshade()) {
                    auto request_capture = request_reshade_screen_capture(capture_screenshot_callback, 11);
                    if (request_capture == 1) {
                        API::get()->log_info("Request capture %d", request_capture);
                    } else {
                        API::get()->log_info("Request capture failed %d", request_capture);
                        finished_capture = true;
                    }
                }
                else {
                    API::get()->log_info("Failed to load reshade module");
                }
            }

            increment_hide_frame();
        }

        if (*capture_state_ptr == SAVECAPTURESTATE_SAVE_CAPTURE && !is_saved) {
            API::get()->log_info("We are in: %d", *capture_state_ptr);

            auto serializedResult = obj->get_field<API::ManagedObject*>("_SerializedResult");
            auto method = API::get()->tdb()->find_method("via.render.SerializedResult", "get_Content");
            auto captureData = method->call<API::ManagedObject*>(API::get()->get_vm_context(), *serializedResult);

            API::get()->log_info("captureData available %d",captureData != nullptr);

            auto captureDataLength = captureData->call<int>("GetLength", API::get()->get_vm_context(), captureData);
            API::get()->log_info("CaptureDataLength finish");
            API::get()->log_info("CaptureData Length: %d", captureDataLength);

            std::vector<byte> data(captureDataLength);

            for (int i = 0; i < data.size(); i++) {
                auto result = captureData->call<byte>("Get", API::get()->get_vm_context(), captureData, i);
                data[i] = result;
            }

            auto file = std::ofstream("D:\\capture.webp", std::ios::binary | std::ios::out | std::ios::trunc);
            file.write((char*)&data[0], data.size());
            file.flush();
            file.close();

            // Inject our own Joker image
            auto file2 = std::ifstream("D:\\capture_test_write.webp", std::ios::binary | std::ios::in);
            auto file_size = 0;

            file2.seekg(0, std::ios::end);
            file_size = file2.tellg();

            file2.seekg(0, std::ios::beg);

            std::vector<byte> data2(file_size);
            file2.read((char*)&data2[0], data2.size());

            if (finished_capture && copied_buffer != nullptr) {
                file_size = copied_buffer->size();
                data2.resize(file_size);
                std::copy(copied_buffer->begin(), copied_buffer->end(), data2.begin());
            
                std::ofstream file3("D:\\real_test.webp", std::ios::binary | std::ios::out | std::ios::trunc);
                file3.write((char*)&data2[0], data2.size());
                file3.flush();
                file3.close();
            }

            auto obj2 = API::get()->create_managed_array(API::get()->tdb()->find_type("System.Byte"), file_size);
            obj2->add_ref();

            auto method_set = obj2->get_type_definition()->find_method("Set");

            for (int i = 0; i < data2.size(); i++) {
                obj2->call("Set", API::get()->get_vm_context(), obj2, i, data2[i]);
            }

            API::get()->log_info("Pointer array %p vs get myself %p", (void*)captureData, (void*)(*(API::ManagedObject**)((std::uint8_t*)(*serializedResult) + SerializeResultContentArrayOffset)));
            *(API::ManagedObject**)((std::uint8_t*)(*serializedResult) + SerializeResultContentArrayOffset) = obj2;
            captureData->release();

            auto new_content = method->call<API::ManagedObject*>(API::get()->get_vm_context(), *serializedResult);
            API::get()->log_info("new_content full name %s, old content full name %s", new_content->get_type_definition()->get_full_name().c_str(), captureData->get_type_definition()->get_full_name().c_str());
            API::get()->log_info("new content ref count %d old content ref count %d", new_content->get_ref_count(), captureData->get_ref_count());
            is_saved = true;
        }

        if (is_saved && *capture_state_ptr == SAVECAPTURESTATE_IDLE) {
            is_saved = false;
            has_custom_screenshot_started = false;
        }
    }
}

int pre_start_create_cphoto(int argc, void** argv, REFrameworkTypeDefinitionHandle* arg_tys, unsigned long long ret_addr) {
    return REFRAMEWORK_HOOK_CALL_ORIGINAL;
}

void post_start_create_cphoto(void** ret_val, REFrameworkTypeDefinitionHandle ret_ty, unsigned long long ret_addr) {
    auto cphoto = *(API::ManagedObject**)ret_val;
    auto arr1 = cphoto->get_field<API::ManagedObject*>("SerializeData");
    (*arr1)->release();

    auto new_arr = API::get()->create_managed_array(API::get()->tdb()->find_type("System.Byte"), MaxSerializePhotoSize);
    new_arr->add_ref();

    (*arr1) = new_arr;
}

int pre_start_create_album_save_param(int argc, void** argv, REFrameworkTypeDefinitionHandle* arg_tys, unsigned long long ret_addr) {
    return REFRAMEWORK_HOOK_CALL_ORIGINAL;
}

void post_start_create_album_save_param(void** ret_val, REFrameworkTypeDefinitionHandle ret_ty, unsigned long long ret_addr) {
    auto album_save_param = *(API::ManagedObject**)ret_val;
    auto photo_datas_ptr = album_save_param->get_field<API::ManagedObject*>("_PhotoDatas");
    auto photo_datas = *photo_datas_ptr;

    auto photo_data_length = photo_datas->call<int>("GetLength", API::get()->get_vm_context(), photo_datas);
    for (int i = 0; i < photo_data_length; i++) {
        auto photo_data = photo_datas->call<API::ManagedObject*>("Get", API::get()->get_vm_context(), photo_datas, i);
        auto photo_data_ptr = photo_data->get_field<API::ManagedObject*>("SerializeData");
        auto photo_data_obj = *photo_data_ptr;
        auto new_photo_data_obj = API::get()->create_managed_array(API::get()->tdb()->find_type("System.Byte"), MaxSerializePhotoSize);
        new_photo_data_obj->add_ref();
        *photo_data_ptr = new_photo_data_obj;
    }
}

bool is_photo_save = false;

int pre_start_savedatamanager_system_requestphotosave(int argc, void** argv, REFrameworkTypeDefinitionHandle* arg_tys, unsigned long long ret_addr) {
    is_photo_save = true;
    API::get()->log_info("Request photo save");
    return REFRAMEWORK_HOOK_CALL_ORIGINAL;
}

void post_start_savedatamanager_system_requestphotosave(void** ret_val, REFrameworkTypeDefinitionHandle ret_ty, unsigned long long ret_addr) {
    
}

API::ManagedObject *csave_obj = nullptr;

int pre_start_csave_request_ctor(int argc, void** argv, REFrameworkTypeDefinitionHandle* arg_tys, unsigned long long ret_addr) {
    csave_obj = (API::ManagedObject*)argv[1];
    return REFRAMEWORK_HOOK_CALL_ORIGINAL;
}

void post_start_csave_request_ctor(void** ret_val, REFrameworkTypeDefinitionHandle ret_ty, unsigned long long ret_addr) {
    if (is_photo_save) {
        if (csave_obj != nullptr) {
            csave_obj->call("set_TargetSize", API::get()->get_vm_context(), csave_obj, 0x120000);
        } else {
            API::get()->log_info("CSaveRequest created null");
        }

        is_photo_save = false;
    }
}

bool on_pre_gui_draw_element(void*, void*) {
    if (is_in_hide_frame()) {
        return false;
    }
    else {
        return true;
    }
}

extern "C" __declspec(dllexport) void reframework_plugin_required_version(REFrameworkPluginVersion* version) {
    version->major = REFRAMEWORK_PLUGIN_VERSION_MAJOR;
    version->minor = REFRAMEWORK_PLUGIN_VERSION_MINOR;
    version->patch = REFRAMEWORK_PLUGIN_VERSION_PATCH;

    // Optionally, specify a specific game name that this plugin is compatible with.
    version->game_name = "MHWILDS";
}

extern "C" __declspec(dllexport) bool reframework_plugin_initialize(const REFrameworkPluginInitializeParam* param) {
    API::initialize(param);
    const auto functions = param->functions;
    functions->on_imgui_draw_ui(on_draw_ui);
    functions->on_pre_gui_draw_element(on_pre_gui_draw_element);
    functions->on_pre_application_entry

    auto &api = API::get();
    auto tdb = api->tdb();

    auto saveQuestResultMethod = tdb->find_method("app.AlbumManager", "updateSaveCapture");
    saveQuestResultMethod->add_hook(pre_start_update_save_capture, post_start_update_save_capture, false);

    auto createCPhotoMethod = tdb->find_method("app.savedata.cPhoto", "create");
    createCPhotoMethod->add_hook(pre_start_create_cphoto, post_start_create_cphoto, false);

    auto createAlbumSaveParamMethod = tdb->find_method("app.savedata.cAlbumSaveParam", "create");
    createAlbumSaveParamMethod->add_hook(pre_start_create_album_save_param, post_start_create_album_save_param, false);

    auto photoSaveMethod = tdb->find_method("app.SaveDataManager", "systemRequestPhotoSave");
    photoSaveMethod->add_hook(pre_start_savedatamanager_system_requestphotosave, post_start_savedatamanager_system_requestphotosave, false);

    auto csaveRequestMethod = tdb->find_method("ace.SaveDataManagerBase.cSaveRequest", ".ctor");
    csaveRequestMethod->add_hook(pre_start_csave_request_ctor, post_start_csave_request_ctor, false);

    return true;
}

