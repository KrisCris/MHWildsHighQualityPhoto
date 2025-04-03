// Install DebugView to view the OutputDebugString calls
#define CIMGUI_DEFINE_ENUMS_AND_STRUCTS
#define CIMGUI_NO_EXPORT

#include <sstream>
#include <mutex>
#include <cimgui.h>
#include <filesystem>

#undef API

#include "Plugin.hpp"
#include "MHWildsTypes.h"
#include "ModSettings.hpp"
#include "InjectClient/ReShadeAddOnInjectClient.hpp"
#include "WebPCaptureInjector.hpp"
#include "REFrameworkBorrowedAPI.hpp"

#include "GameUIController.hpp"

#include <nfd.hpp>

std::unique_ptr<Plugin> plugin_instance = nullptr;

static bool is_override_image_path_valid(const std::string& path, bool limit_size) {
    if (path.empty()) {
        return false;
    }

    if (!std::filesystem::exists(path)) {
        return false;
    }

    if (limit_size && std::filesystem::file_size(path) > MaxSerializePhotoSizeOriginal) {
        return false;
    }

    // No idea if this file is WebP or not, either way, the game will let us know (by corrupt cat icon)
    return true;
}

void Plugin::decide_and_set_screen_cap_or_override_inject(std::unique_ptr<FileInjectClient> &override_client,
    bool should_override,
    const std::string &override_path,
    bool should_lossless,
    bool is_photo_mode) {
    auto mod_settings = ModSettings::get_instance();
    auto injector = WebPCaptureInjector::get_instance();

    if (mod_settings == nullptr || injector == nullptr) {
        return;
    }

    auto reshade_addon_client = ReShadeAddOnInjectClient::get_instance();
    if (mod_settings->disable_mod) {
        reshade_addon_client->set_enable(false);
        injector->set_inject_client(null_capture_client.get());
    
        return;
    }

    reshade_addon_client->set_enable(!mod_settings->is_disable_high_quality_screen_capture());
    reshade_addon_client->set_lossless(should_lossless);
    reshade_addon_client->set_use_old_limit_size(false);

    if (is_photo_mode) {
        reshade_addon_client->set_is_photo_mode(true);

        if (!mod_settings->is_disable_high_quality_screen_capture()) {
            if (mod_settings->get_photo_mode_image_quality() == PhotoModeImageQuality_DoNotModify) {
                reshade_addon_client->set_enable(false);
            }
        }

        if (mod_settings->get_photo_mode_image_quality() == PhotoModeImageQuality_LowQualityApplyFilters) {
            reshade_addon_client->set_use_old_limit_size(true);
        }
    } else {
        reshade_addon_client->set_is_photo_mode(false);
    }

    injector->set_inject_client(reshade_addon_client);

    if (should_override) {
        if (is_override_image_path_valid(override_path, is_photo_mode)) {
            override_client->set_file_path(override_path);
            injector->set_inject_client(override_client.get());

            reshade_addon_client->set_enable(false);
        }
    }
}

int Plugin::pre_quest_failure_hook(int argc, void** argv, REFrameworkTypeDefinitionHandle* arg_tys, unsigned long long ret_addr) {
    auto mod_settings = ModSettings::get_instance();

    if (mod_settings == nullptr || plugin_instance == nullptr) {
        return REFRAMEWORK_HOOK_CALL_ORIGINAL;
    }

    plugin_instance->decide_and_set_screen_cap_or_override_inject(plugin_instance->quest_failure_force_client,
        mod_settings->enable_override_quest_failure,
        mod_settings->override_quest_failure_background_path,
        mod_settings->use_lossless_image_for_quest_result,
        false);

    return REFRAMEWORK_HOOK_CALL_ORIGINAL;
}

void Plugin::post_quest_failure_hook(void** ret_val, REFrameworkTypeDefinitionHandle ret_ty, unsigned long long ret_addr) {

}

int Plugin::pre_quest_success_hook(int argc, void** argv, REFrameworkTypeDefinitionHandle* arg_tys, unsigned long long ret_addr) {
    auto mod_settings = ModSettings::get_instance();

    if (mod_settings == nullptr || plugin_instance == nullptr) {
        return REFRAMEWORK_HOOK_CALL_ORIGINAL;
    }

    plugin_instance->decide_and_set_screen_cap_or_override_inject(plugin_instance->quest_success_force_client,
        mod_settings->enable_override_quest_success,
        mod_settings->override_quest_complete_background_path,
        mod_settings->use_lossless_image_for_quest_result,
        false);

    return REFRAMEWORK_HOOK_CALL_ORIGINAL;
}

void Plugin::post_quest_success_hook(void** ret_val, REFrameworkTypeDefinitionHandle ret_ty, unsigned long long ret_addr) {

}

int Plugin::pre_quest_cancel_hook(int argc, void** argv, REFrameworkTypeDefinitionHandle* arg_tys, unsigned long long ret_addr) {
    auto mod_settings = ModSettings::get_instance();

    if (mod_settings == nullptr || plugin_instance == nullptr) {
        return REFRAMEWORK_HOOK_CALL_ORIGINAL;
    }

    plugin_instance->decide_and_set_screen_cap_or_override_inject(plugin_instance->quest_cancel_force_client,
        mod_settings->enable_override_quest_cancel,
        mod_settings->override_quest_headback_background_path,
        mod_settings->use_lossless_image_for_quest_result,
        false);

    return REFRAMEWORK_HOOK_CALL_ORIGINAL;
}

void Plugin::post_quest_cancel_hook(void** ret_val, REFrameworkTypeDefinitionHandle ret_ty, unsigned long long ret_addr) {

}

int Plugin::pre_save_capture_photo_hook(int argc, void** argv, REFrameworkTypeDefinitionHandle* arg_tys, unsigned long long ret_addr) {
    auto mod_settings = ModSettings::get_instance();

    if (mod_settings == nullptr) {
        return REFRAMEWORK_HOOK_CALL_ORIGINAL;
    }

    plugin_instance->decide_and_set_screen_cap_or_override_inject(plugin_instance->album_photo_force_client,
        mod_settings->enable_override_album_image,
        mod_settings->override_album_image_path,
        false,
        true);

    return REFRAMEWORK_HOOK_CALL_ORIGINAL;
}

void Plugin::post_save_capture_photo_hook(void** ret_val, REFrameworkTypeDefinitionHandle ret_ty, unsigned long long ret_addr) {

}

void Plugin::update() {
    auto game_ui_controller_instance = GameUIController::get_instance();
    if (game_ui_controller_instance != nullptr) {
        game_ui_controller_instance->update();
    }

    auto reshade_addon_client = ReShadeAddOnInjectClient::get_instance();
    if (reshade_addon_client != nullptr) {
        reshade_addon_client->update();
    }
}

void Plugin::open_blocking_webp_pick_file_dialog(std::string &file_path) {
    nfdu8char_t *out_path;
    nfdu8filteritem_t filters[2] = { { "WebP image", "webp" }, { "All files", "*" } };

    if (NFD::OpenDialog(out_path, filters, 2) == NFD_OKAY) {
        file_path = out_path;
        NFD::FreePath(out_path);
    } else {
        file_path.clear();
    }
}

void Plugin::draw_user_interface_path(const std::string &label, std::string &target_path, bool limit_size) {
    igText(label.c_str());
    igSameLine(0.0f, 5.0f);

    std::string id_input = std::format("##{}", label);
    std::string name_button = std::format("Browse##{}", label);

    igInputText(id_input.c_str(), const_cast<char*>(target_path.c_str()), target_path.size() + 1,
        ImGuiInputTextFlags_ReadOnly, nullptr, nullptr);

    igSameLine(0.0f, 5.0f);

    if (igButton(name_button.c_str(), ImVec2(0, 0))) {
        open_blocking_webp_pick_file_dialog(target_path);
    }

    if (!target_path.empty()) {
        if (!is_override_image_path_valid(target_path, limit_size)) {
            std::string error_message = std::format("Your WebP image is either too big (size > {} KB), or corrupted or not exist anymore. Please recheck",
                MaxSerializePhotoSizeOriginal / 1024);

            igPushStyleColor_Vec4(ImGuiCol_Text, ImVec4(1.0f, 0.0f, 0.0f, 1.0f));
            igText(error_message.c_str());
            igPopStyleColor(1);
        }
    }
}

static const char *get_photo_mode_image_quality_name(PhotoModeImageQuality quality) {
    switch (quality) {
        case PhotoModeImageQuality_HighQualityApplyFilters:
            return "High quality + ReShade";
        case PhotoModeImageQuality_LowQualityApplyFilters:
            return "Low quality + ReShade";
        case PhotoModeImageQuality_DoNotModify:
            return "Low quality (Same as without mod)";
        default:
            return "Unknown";
    }
}

static void igTextBulletWrapped(const char *bullet, const char *text) {
    igTextWrapped(bullet);
    igSameLine(0.0f, 5.0f);
    igTextWrapped(text);
}

void Plugin::draw_user_interface() {
    if (igCollapsingHeader_TreeNodeFlags("Custom Photos", ImGuiTreeNodeFlags_None)) {
        auto mod_settings = ModSettings::get_instance();
        auto mod_settings_copy = *mod_settings;

        igPushStyleColor_Vec4(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 0.0f, 1.0f));
        igTextWrapped("WARNING: HDR not tested on a real HDR monitor. Do not enable this mod when use 21:9 on a 16:9 monitor, your high quality photo will have black bars on top and bottom");
        igPopStyleColor(1);

        auto reshade_addon_client = ReShadeAddOnInjectClient::get_instance();
        if (reshade_addon_client == nullptr || !reshade_addon_client->is_reshade_present()) {
            igPushStyleColor_Vec4(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 0.0f, 1.0f));
            igTextWrapped("WARNING: ReShade with Add-On is not present. High quality image capture will not work. If you want high quality capture to work, please install Reshade WITH ADD-ON (CAPS LOCK TEXT IMPORTANT)");
            igPopStyleColor(1);
        }

        igSeparator();

        if (igTreeNode_Str("Help")) {
            if (igTreeNode_Str("How to upload custom image to hunter profile")) {
                igTextBulletWrapped("1.", "Go to the \"Photo mode/Album\" section and enable \"Override Album Image\" option.");
                igTextBulletWrapped("2.", "Take an image in Photo Mode. Your custom image will then be saved in the \"Album\".");
                igTextBulletWrapped("3.", "Go to your Hunter Profile, choose the custom image just saved in the \"Album\".");
                igTreePop();
            }

#ifdef ENABLE_HIGH_QUALITY_PHOTO_MODE
            if (igTreeNode_Str("Photo mode: High quality + Reshade HIGH RISK")) {
                igTextWrapped("- The game by default stores all quest result/album image in WebP, with size 1920x1080 and file size <= 256KB.");
                igTextWrapped("- By enabling high quality photo mode, the mod will expands each photo's maximum file size from 256KB to 960KB.");
                
                if (igTreeNode_Str("Advantages")) {
                    igTextWrapped("* Higher image quality, less blocky artifacts in a lot of situations.");
                    igTreePop();
                }

                if (igTreeNode_Str("Disadvantages")) {
                    igTextWrapped("* Photo will corrupt when you delete this mod.");
                    igTextWrapped("* Other people that does not install this mod can't see your high quality photo.");
                    igTextWrapped("* Corrupt image may have chance to corrupt your save when leave undeleted.");
                    igTextWrapped("* Use more storage (usually 800-900KB) for each photo. A good trade off but with those three points upper, is extremely less appetising");

                    igTreePop();
                }
                
                if (igTreeNode_Str("Recommended actions")) {
                    igTextWrapped("* I really don't recommend using this. But someone may use it, so I leave it in");
                    igTextWrapped("* If you want to use this, please make sure you have a backup of your save file.");
                    igTextWrapped("* Use this to send screenshot with ReShade applied to Steam maybe (capture in photo mode, then F12).");

                    
                    igTreePop();
                }

                igTreePop();
            }
#endif

            igTreePop();
        }

        if (igTreeNode_Str("General")) {
            igCheckbox("Disable Mod ##DisableMod", &mod_settings->disable_mod);
            
#ifdef ENABLE_HIGH_QUALITY_PHOTO_MODE
            igCheckbox("Disable High Quality Screen Capture##DisableHighQualityScreenCapture", &mod_settings->disable_high_quality_screen_capture);

            if (igIsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
                igSetTooltip("Your quest result background will not be changed by the mod");
            }
#endif

            igText("HDR Bits");
            if (igIsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
                igSetTooltip("The HDR bits used for screen capture. Valid on HDR monitor");
            }

            igSameLine(0.0f, 5.0f);
            igInputInt("##HDRBits", &mod_settings->hdr_bits, 1, 1, ImGuiInputTextFlags_None);

            igTreePop();
        }

        if (igTreeNode_Str("Photo mode/Album")) {
            igCheckbox("Enable Override Album Image", &mod_settings->enable_override_album_image);

            if (igIsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
                igSetTooltip("Your photo mode result will be a image you selected. You can use this to upload to hunter profile");
            }
    
            if (mod_settings->enable_override_album_image) {
                igPushStyleColor_Vec4(ImGuiCol_Text, ImVec4(1.0f, 0.0f, 0.0f, 1.0f));
                igTextWrapped("NOTE: Please adjust your custom image to be in WebP format. The image dimension should be 1920x1080 and file size <= 256KB.");
                igPopStyleColor(1);

                draw_user_interface_path("Album Image Path", mod_settings->override_album_image_path, true);
            }

#ifdef ENABLE_HIGH_QUALITY_PHOTO_MODE
            if (mod_settings->disable_high_quality_screen_capture) {
                igBeginDisabled(true);
            }

            igText("Photo Mode Image Quality");

            if (igIsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled) && mod_settings->disable_high_quality_screen_capture) {
                igSetTooltip("The option is disabled because another option: \"Disable High Quality Screen Capture\" (\"General\" section) is enabled");
            }

            igSameLine(0.0f, 5.0f);

            if (igBeginCombo("##PhotoModeImageQuality", get_photo_mode_image_quality_name(mod_settings->photo_mode_image_quality), ImGuiComboFlags_None)) {
                bool selected = false;

                if (igSelectable_BoolPtr("Low quality (Same as without mod)", &selected, ImGuiSelectableFlags_None, ImVec2(0, 0))) {
                    mod_settings->photo_mode_image_quality = PhotoModeImageQuality_DoNotModify;
                }

                if (igSelectable_BoolPtr("Low quality + ReShade", &selected, ImGuiSelectableFlags_None, ImVec2(0, 0))) {
                    mod_settings->photo_mode_image_quality = PhotoModeImageQuality_LowQualityApplyFilters;
                }

                if (igSelectable_BoolPtr("High quality + ReShade", &selected, ImGuiSelectableFlags_None, ImVec2(0, 0))) {
                    mod_settings->photo_mode_image_quality = PhotoModeImageQuality_HighQualityApplyFilters;
                }

                igEndCombo();
            }

            if (mod_settings->photo_mode_image_quality == PhotoModeImageQuality_LowQualityApplyFilters ||
                mod_settings->photo_mode_image_quality == PhotoModeImageQuality_HighQualityApplyFilters) {
                if (reshade_addon_client == nullptr || !reshade_addon_client->is_reshade_present()) {
                    igPushStyleColor_Vec4(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 0.0f, 1.0f));
                    igTextWrapped("WARNING: ReShade with Add-On is not present. High quality image capture will not work.");
                    igPopStyleColor(1);
                }
            }

            if (mod_settings->photo_mode_image_quality == PhotoModeImageQuality_HighQualityApplyFilters) {
                igPushStyleColor_Vec4(ImGuiCol_Text, ImVec4(1.0f, 0.0f, 0.0f, 1.0f));
                igTextWrapped("WARNING: THIS IS A HIGH RISK OPTION. See more in \"Help\" section");
                igPopStyleColor(1);
            }

            if (mod_settings->disable_high_quality_screen_capture) {
                igEndDisabled();
            }

            igText("Max Album Image Quality");
            if (igIsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
                igSetTooltip("Your high quality photo mode image will be compressed. The higher the quality is, the more the image is preserved");
            }

            igSameLine(0.0f, 5.0f);
            igSliderInt("##MaxAlbumImageQuality", &mod_settings->max_album_image_quality, 10, 100, "%d%%", ImGuiSliderFlags_None);
#endif

            igTreePop();
        }

        if (igTreeNode_Str("Quest Result/Hunter Highlights")) {
            igPushStyleColor_Vec4(ImGuiCol_Text, ImVec4(1.0f, 0.0f, 0.0f, 1.0f));
            igTextWrapped("NOTE: Please adjust your custom image to be in WebP format. The image dimension should be 1920x1080 on 16x9 monitor and 2560x1080 on 21x9 monitor. File size is not limited");
            igPopStyleColor(1);

            igCheckbox("Enable Override Quest Failure Image", &mod_settings->enable_override_quest_failure);
    
            if (mod_settings->enable_override_quest_failure) {
                draw_user_interface_path("Quest Failure Image Path", mod_settings->override_quest_failure_background_path, false);
            }
    
            igCheckbox("Enable Override Quest Success Image", &mod_settings->enable_override_quest_success);
    
            if (mod_settings->enable_override_quest_success) {
                draw_user_interface_path("Quest Success Image Path", mod_settings->override_quest_complete_background_path, false);
            }
    
            igCheckbox("Enable Override Quest Cancel Image", &mod_settings->enable_override_quest_cancel);
    
            if (mod_settings->enable_override_quest_cancel) {
                draw_user_interface_path("Quest Cancel Image Path", mod_settings->override_quest_headback_background_path, false);
            }
    
            igCheckbox("Use Lossless Image for Quest Result##UseLosslessQuestResult", &mod_settings->use_lossless_image_for_quest_result);
            if (igIsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
                igSetTooltip("This will result in crisp quest result background, but will create a visible stutter in case the lossless image size is heavy (eg 4K image)");
            }

            igTreePop();
        }

        if (igTreeNode_Str("Debug")) {
            igCheckbox("Dump Original WebP image", &mod_settings->dump_original_webp);
            if (igIsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
                igSetTooltip("This will dump the original WebP image the game made to the game directory.");
            }

            igText("Path to WebP: <GameDir>/reframework/data/MHWilds_HighQualityPhotoMod_OriginalImage.webp");
            igTreePop();
        }

        mod_settings->max_album_image_quality = std::clamp(mod_settings->max_album_image_quality, 10, 100);
        mod_settings->hdr_bits = std::clamp(mod_settings->hdr_bits, 10, 20);

        if (mod_settings->data_changed(mod_settings_copy)) {
            mod_settings->save();
        }
    }
}

Plugin::Plugin(const REFrameworkPluginInitializeParam *params) {
    auto& api = reframework::API::get();
    auto tdb = api->tdb();

    auto quest_failed_method = tdb->find_method("app.cQuestFailed", "enter");
    quest_failed_method->add_hook(pre_quest_failure_hook, post_quest_failure_hook, false);

    auto quest_success_method = tdb->find_method("app.cQuestClear", "enter");
    quest_success_method->add_hook(pre_quest_success_hook, post_quest_success_hook, false);

    auto quest_cancel_method = tdb->find_method("app.cQuestCancel", "enter");
    quest_cancel_method->add_hook(pre_quest_cancel_hook, post_quest_cancel_hook, false);

    auto save_capture_method = tdb->find_method("app.AlbumManager", "saveCapturePhoto");
    save_capture_method->add_hook(pre_save_capture_photo_hook, post_save_capture_photo_hook, false);

    quest_success_force_client = std::make_unique<FileInjectClient>();
    quest_failure_force_client = std::make_unique<FileInjectClient>();
    quest_cancel_force_client = std::make_unique<FileInjectClient>();
    album_photo_force_client = std::make_unique<FileInjectClient>();
    null_capture_client = std::make_unique<NullCaptureInjectClient>();

    quest_success_force_client->set_limit_size(false);
    quest_failure_force_client->set_limit_size(false);
    quest_cancel_force_client->set_limit_size(false);
    album_photo_force_client->set_limit_size(true);
}

void Plugin::initialize(const REFrameworkPluginInitializeParam *params) {
    plugin_instance = std::make_unique<Plugin>(params);

    auto persistent_dir = REFramework::get_persistent_dir();
    
    if (!std::filesystem::exists(persistent_dir)) {
        std::filesystem::create_directories(persistent_dir);
    }

    auto& api = reframework::API::get();

    /// THIS BELOW MUST BE FIRST
    ModSettings::initialize();
    /// THIS ABOVE MUST BE FIRST

    GameUIController::initialize(params);
    WebPCaptureInjector::initialize(api.get());
    ReShadeAddOnInjectClient::initialize();

    params->functions->on_imgui_draw_ui([](REFImGuiFrameCbData* data) {
        auto plugin = Plugin::get_instance();
        if (plugin != nullptr) {
            plugin->draw_user_interface();
        }
    });

    params->functions->on_pre_application_entry("UpdateBehavior", []() {
        auto plugin = Plugin::get_instance();
        if (plugin != nullptr) {
            plugin->update();
        }
    });
}

Plugin *Plugin::get_instance() {
    return plugin_instance ? plugin_instance.get() : nullptr;
}

extern "C" __declspec(dllexport) void reframework_plugin_required_version(REFrameworkPluginVersion* version) {
    version->major = REFRAMEWORK_PLUGIN_VERSION_MAJOR;
    version->minor = REFRAMEWORK_PLUGIN_VERSION_MINOR;
    version->patch = REFRAMEWORK_PLUGIN_VERSION_PATCH;

    // Optionally, specify a specific game name that this plugin is compatible with.
    version->game_name = "MHWILDS";
}

extern "C" __declspec(dllexport) bool reframework_plugin_initialize(const REFrameworkPluginInitializeParam* param) {
    reframework::API::initialize(param);
    Plugin::initialize(param);

    return true;
}
