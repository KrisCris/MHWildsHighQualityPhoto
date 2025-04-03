#pragma once

#include <reframework/API.hpp>
#include <memory>

#include "InjectClient/FileInjectClient.hpp"
#include "InjectClient/NullInjectClient.hpp"
#include "WebPCaptureInjectClient.hpp"

struct ModSettings;

class Plugin {
private:
    std::unique_ptr<FileInjectClient> quest_success_force_client;
    std::unique_ptr<FileInjectClient> quest_failure_force_client;
    std::unique_ptr<FileInjectClient> quest_cancel_force_client;
    std::unique_ptr<FileInjectClient> album_photo_force_client;
    std::unique_ptr<NullCaptureInjectClient> null_capture_client;

    void draw_user_interface();
    void draw_user_interface_path(const std::string &label, std::string &target_path, bool limit_size);

    void decide_and_set_screen_cap_or_override_inject(std::unique_ptr<FileInjectClient> &override_client,
        bool should_override,
        const std::string &override_path,
        bool should_lossless,
        bool should_force_size);

    void open_blocking_webp_pick_file_dialog(std::string &file_path);

    void update();

private:
    // Hooks
    static int pre_quest_failure_hook(int argc, void** argv, REFrameworkTypeDefinitionHandle* arg_tys, unsigned long long ret_addr);
    static void post_quest_failure_hook(void** ret_val, REFrameworkTypeDefinitionHandle ret_ty, unsigned long long ret_addr);

    static int pre_quest_success_hook(int argc, void** argv, REFrameworkTypeDefinitionHandle* arg_tys, unsigned long long ret_addr);
    static void post_quest_success_hook(void** ret_val, REFrameworkTypeDefinitionHandle ret_ty, unsigned long long ret_addr);

    static int pre_quest_cancel_hook(int argc, void** argv, REFrameworkTypeDefinitionHandle* arg_tys, unsigned long long ret_addr);
    static void post_quest_cancel_hook(void** ret_val, REFrameworkTypeDefinitionHandle ret_ty, unsigned long long ret_addr);

    static int pre_save_capture_photo_hook(int argc, void** argv, REFrameworkTypeDefinitionHandle* arg_tys, unsigned long long ret_addr);
    static void post_save_capture_photo_hook(void** ret_val, REFrameworkTypeDefinitionHandle ret_ty, unsigned long long ret_addr);

public:
    explicit Plugin(const REFrameworkPluginInitializeParam *params);
    ~Plugin() = default;

    static void initialize(const REFrameworkPluginInitializeParam *params);
    static Plugin *get_instance();
};