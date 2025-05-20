#include <reshade.hpp>
#include <sk_hdr_png.hpp>
#include <filesystem>
#include <format>
#include <subprocess.h>
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
#include <fstream>
#include <thread>

#include "Plugin.h"
 
extern "C" __declspec(dllexport) const char *NAME = "High Quality Kill Screen Capturer";
extern "C" __declspec(dllexport) const char *DESCRIPTION = "Take a screenshot when you finish a quest, then send it to the game for displaying";

reshade::api::effect_runtime *current_reshade_runtime = nullptr;
reshade::api::swapchain *current_swapchain = nullptr;

int g_hdr_bit_depths = 11; // Default to 11-bit depth for HDR
ScreenCaptureFinishFunc g_finish_callback = nullptr;
bool screenshot_requested = false;

std::atomic_bool is_hdr_converting = false;
bool g_screenshot_before_reshade = false;

std::string get_current_dll_path() {
    char path[MAX_PATH];
    HMODULE hm = NULL;

    if (GetModuleHandleEx(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
        (LPCSTR) &request_screen_capture, &hm) == 0)
    {
        int ret = GetLastError();
        fprintf(stderr, "GetModuleHandle failed, error = %d\n", ret);
        // Return or however you want to handle an error.
    }
    if (GetModuleFileName(hm, path, sizeof(path)) == 0)
    {
        int ret = GetLastError();
        fprintf(stderr, "GetModuleFileName failed, error = %d\n", ret);
        // Return or however you want to handle an error.
    }

    return std::string(path);
}

static void hdr_convert_thread(std::uint8_t * pixels, std::uint32_t width, std::uint32_t height, reshade::api::format format, int hdr_bit_depths) {
    // Launch this in a separate thread
    // Write to PNG and use tool to convert to SDR, since, if we keep in mind
    auto unique_id = std::chrono::system_clock::now().time_since_epoch().count();
    auto temp_path = std::filesystem::temp_directory_path() / std::format("reshade_hdr_temporary_screenshot{0}.png", unique_id);
    auto temp_path_string = temp_path.string();
    auto temp_path_wstring = temp_path.wstring();
    
#ifdef LOG_DEBUG_STEP
    reshade::log::message(reshade::log::level::debug, "Write HDR screenshot to disk");
#endif

    if (!sk_hdr_png::write_image_to_disk(temp_path_wstring.c_str(),
        width, height,
        reinterpret_cast<void*>(pixels),
        g_hdr_bit_depths,
        format)) {
        if (g_finish_callback) {
            g_finish_callback(RESULT_SCREEN_CAPTURE_HDR_NOT_SAVEABLE, 0, 0, nullptr);
        }
        delete[] pixels; // Clean up the allocated memory after use
        g_finish_callback = nullptr; // Reset the callback to allow new requests
    }
    else {
        delete[] pixels; // Clean up the allocated memory after use

#ifdef LOG_DEBUG_STEP
        reshade::log::message(reshade::log::level::debug, "Call HDR fix to convert to SDR");
#endif

        // Call a program to convert it to friendly SDR PNG
        auto original_dll_containing_path = std::filesystem::path(get_current_dll_path()).parent_path().parent_path();

        auto path_to_exe = (original_dll_containing_path / "hdrfix.exe").string();
        auto path_output = (std::filesystem::temp_directory_path() / std::format("hdrfix_output{0}.png", unique_id)).string();
        const char *command_line[] = { path_to_exe.c_str(), temp_path_string.c_str(), path_output.c_str(), nullptr };

        struct subprocess_s subprocess;
        int result = subprocess_create(command_line, subprocess_option_no_window, &subprocess);

        if (0 != result) {
            if (g_finish_callback) {
                g_finish_callback(RESULT_SCREEN_CAPTURE_HDR_TO_SDR_FAILED, 0, 0, nullptr);
            }
            g_finish_callback = nullptr;
        }

        int process_return_code = 0;
        subprocess_join(&subprocess, &process_return_code);

#ifdef LOG_DEBUG_STEP
        reshade::log::message(reshade::log::level::debug, "HDR convert finished");
#endif

        if (0 != process_return_code) {
            if (g_finish_callback) {
                g_finish_callback(RESULT_SCREEN_CAPTURE_HDR_TO_SDR_FAILED, 0, 0, nullptr);
            }
            g_finish_callback = nullptr;
        } else {
            if (!std::filesystem::exists(path_output)) {
                if (g_finish_callback) {
                    g_finish_callback(RESULT_SCREEN_CAPTURE_HDR_TO_SDR_FAILED, 0, 0, nullptr);
                }
            } else {
                // Read the output file and extract it
                auto stream = std::ifstream(path_output, std::ios::binary | std::ios::ate);
                auto size = stream.tellg();

                stream.seekg(0, std::ios::beg);
                std::vector<std::uint8_t> data(size);

                stream.read(reinterpret_cast<char*>(data.data()), size);
                stream.close();

                int width, height;
                int comp = 4;
                stbi_uc *data_result = stbi_load_from_memory(data.data(), static_cast<int>(size), &width, &height, &comp, 4); // Assuming 4 bytes per pixel (RGBA)
            
                if (data_result) {
#ifdef LOG_DEBUG_STEP
                    reshade::log::message(reshade::log::level::debug, "Reading HDR convert OK, sending to callback");
#endif

                    if (g_finish_callback) {
                        g_finish_callback(RESULT_SCREEN_CAPTURE_SUCCESS, width, height, data_result);
                    }
                    stbi_image_free(data_result); // Free the loaded image data
                } else {
                    if (g_finish_callback) {
                        g_finish_callback(RESULT_SCREEN_CAPTURE_HDR_NOT_SAVEABLE, 0, 0, nullptr);
                    }
                }
            }
            g_finish_callback = nullptr; // Reset the callback to allow new requests
        }
    }

    is_hdr_converting = false;
}

static bool is_screenshot_requested() {
    return screenshot_requested;
}

static void capture_screenshot_impl();

static void on_present_without_effects_applied(reshade::api::command_queue *queue, reshade::api::swapchain *swapchain, const reshade::api::rect *source_rect,
    const reshade::api::rect *dest_rect, uint32_t dirty_rect_count, const reshade::api::rect *dirty_rect) {
    current_swapchain = swapchain;

    if (is_screenshot_requested()) {
        if (g_screenshot_before_reshade) {
            capture_screenshot_impl();
        }
    }
}

static void on_present_with_effects_applied(reshade::api::effect_runtime *runtime)
{
    current_reshade_runtime = runtime;

    if (is_screenshot_requested()) {
        if (!g_screenshot_before_reshade) {
            capture_screenshot_impl();
        }
    }
}

static void capture_screenshot_impl() {
    if (!is_screenshot_requested()) {
        return;
    }

    if (is_hdr_converting) {
        return;
    }

    screenshot_requested = false;

    auto back_buffer = current_reshade_runtime->get_current_back_buffer();
    auto resource_description = current_reshade_runtime->get_device()->get_resource_desc(back_buffer);

    auto format = reshade::api::format_to_default_typed(resource_description.texture.format);
    auto color_space = current_swapchain->get_color_space();

    bool is_hdr = (format == reshade::api::format::r16g16b16a16_float) ||
                  ((format == reshade::api::format::b10g10r10a2_unorm) || (format == reshade::api::format::r10g10b10a2_unorm)) && (color_space == reshade::api::color_space::hdr10_st2084);

    std::uint32_t width, height;
    current_reshade_runtime->get_screenshot_width_and_height(&width, &height);
    
    auto bytes_per_pixel = (format == reshade::api::format::r16g16b16a16_float) ? 8 : 4; // 4 bytes for RGBA, 8 bytes for HDR scRGB
    auto pixels = new uint8_t[width * height * bytes_per_pixel]; // Assuming 4 bytes per pixel (RGBA)

#ifdef LOG_DEBUG_STEP
    reshade::log::message(reshade::log::level::debug, "Start capturing screenshot");
#endif

    if (!current_reshade_runtime->capture_screenshot(pixels)) {
        if (g_finish_callback) {
            g_finish_callback(RESULT_SCREEN_RESHADE_CAPTURE_FAILURE, 0, 0, nullptr);
        }
        delete[] pixels; // Clean up the allocated memory after use
        g_finish_callback = nullptr; // Reset the callback to allow new requests
        return;
    }

#ifdef LOG_DEBUG_STEP
    reshade::log::message(reshade::log::level::debug, "Capturing screenshot finished");
#endif

    if (!is_hdr) {
#ifdef LOG_DEBUG_STEP
        reshade::log::message(reshade::log::level::debug, "Screenshot is not HDR, sending it directly");
#endif

        if (g_finish_callback) {
            g_finish_callback(RESULT_SCREEN_CAPTURE_SUCCESS, width, height, pixels);
        }
        delete[] pixels; // Clean up the allocated memory after use
        g_finish_callback = nullptr; // Reset the callback to allow new requests
        return;
    } else {
#ifdef LOG_DEBUG_STEP
        reshade::log::message(reshade::log::level::debug, "Screenshot is HDR, launching conversion to SDR");
#endif

        // Launch a thread to convert the HDR image to SDR
        is_hdr_converting = true;
        std::thread(hdr_convert_thread, pixels, width, height, format, g_hdr_bit_depths).detach();
        return;
    }
}

extern "C" int request_screen_capture(ScreenCaptureFinishFunc finish_callback, int hdr_bit_depths, bool screenshot_before_reshade) {
    if (g_finish_callback) {
        return RESULT_SCREEN_CAPTURE_IN_PROGRESS;
    }

    g_finish_callback = finish_callback;
    g_hdr_bit_depths = hdr_bit_depths;
    g_screenshot_before_reshade = screenshot_before_reshade;
    screenshot_requested = true;

    return RESULT_SCREEN_CAPTURE_SUBMITTED;
}

extern "C" void set_reshade_filters_enable(bool should_enable) {
    current_reshade_runtime->set_effects_state(should_enable);
}

BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID)
{
    switch (fdwReason)
    {
    case DLL_PROCESS_ATTACH:
        // Call 'reshade::register_addon()' before you call any other function of the ReShade API.
        // This will look for the ReShade instance in the current process and initialize the API when found.
        if (!reshade::register_addon(hinstDLL))
            return FALSE;
        // This registers a callback for the 'present' event, which occurs every time a new frame is presented to the screen.
        // The function signature has to match the type defined by 'reshade::addon_event_traits<reshade::addon_event::present>::decl'.
        // For more details check the inline documentation for each event in 'reshade_events.hpp'.
        reshade::register_event<reshade::addon_event::present>(&on_present_without_effects_applied);
        reshade::register_event<reshade::addon_event::reshade_present>(&on_present_with_effects_applied);
        break;
    case DLL_PROCESS_DETACH:
        // Optionally unregister the event callback that was previously registered during process attachment again.
        reshade::unregister_event<reshade::addon_event::present>(&on_present_without_effects_applied);
        reshade::unregister_event<reshade::addon_event::reshade_present>(&on_present_with_effects_applied);
        // And finally unregister the add-on from ReShade (this will automatically unregister any events and overlays registered by this add-on too).
        reshade::unregister_addon(hinstDLL);
        break;
    }
    return TRUE;
}