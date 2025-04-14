#pragma once

#include <reframework/API.hpp>
#include <memory>

#include "InjectClient/FileInjectClient.hpp"
#include "InjectClient/NullInjectClient.hpp"
#include "WebPCaptureInjectClient.hpp"

#include "PluginBase.hpp"

struct ModSettings;

class Plugin_QuestResult : PluginBase {
private:
    std::unique_ptr<FileInjectClient> album_photo_force_client;
    std::unique_ptr<NullCaptureInjectClient> null_capture_client;

    void set_capture_again();

private:
    // Hooks
    static int pre_save_capture_photo_hook(int argc, void** argv, REFrameworkTypeDefinitionHandle* arg_tys, unsigned long long ret_addr);
    static void post_save_capture_photo_hook(void** ret_val, REFrameworkTypeDefinitionHandle ret_ty, unsigned long long ret_addr);

protected:
    void update() override;
    void draw_user_interface() override;

public:
    explicit Plugin_QuestResult(const REFrameworkPluginInitializeParam *params);
    ~Plugin_QuestResult() = default;

    static void initialize(const REFrameworkPluginInitializeParam *params);
    static Plugin_QuestResult *get_instance();
};