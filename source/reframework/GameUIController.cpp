#include "GameUIController.hpp"
#include "reframework/API.hpp"
#include "ModSettings.hpp"
#include <memory>

using namespace reframework;

std::unique_ptr<GameUIController> game_ui_controller_instance = nullptr;

bool game_ui_controller_on_pre_gui_draw_element(void *obj_void, void* context) {
    if (game_ui_controller_instance == nullptr) {
        return true;
    }

    // Return true to draw the UI
    if (game_ui_controller_instance->is_in_hide()) {
        return false;
    }

    auto settings = ModSettings::get_instance();

    // Check for our lovely notification icon
    if (game_ui_controller_instance->get_is_in_quest_result() && settings->hide_chat_notification) {
        auto obj = reinterpret_cast<reframework::API::ManagedObject*>(obj_void);
        auto game_obj = obj->call("get_GameObject", context, obj);

        if (game_obj == game_ui_controller_instance->get_notification_gameobject()) {
            return false;
        }
    }

    return true;
}

GameUIController::GameUIController(const REFrameworkPluginInitializeParam* initialize_params) {
    if (initialize_params != nullptr) {
        initialize_params->functions->on_pre_gui_draw_element(game_ui_controller_on_pre_gui_draw_element);
    }

    auto &api = reframework::API::get();
    auto tdb = api->tdb();

    auto on_awake_chat = tdb->find_method("app.GUI000020", "guiAwake");
    on_awake_chat->add_hook(pre_gui000020_on_gui_awake, post_gui000020_on_gui_awake, false);
}

void GameUIController::hide_for(int frame_count) {
    if (is_in_hide()) {
        API::get()->log_info("Already hiding, ignoring request to hide for %d frames", frame_count);
        return;
    } else {
        hide_frames_passed = 0;
        total_hide_frame = frame_count;
    }
}

void GameUIController::show_impl() {
    if (is_in_hide()) {
        hide_frames_passed = -1;
    } else {
        API::get()->log_info("Not hiding, ignoring request to show");
    }
}

float GameUIController::get_hiding_progress() const {
    if (!is_in_hide() || is_hiding_until_notice()) {
        return 0.0f;
    }

    return static_cast<float>(hide_frames_passed) / static_cast<float>(total_hide_frame);
}

GameUIController *GameUIController::get_instance() {
    return game_ui_controller_instance ? game_ui_controller_instance.get() : nullptr;
}

void GameUIController::initialize(const REFrameworkPluginInitializeParam *initialize_params) {
    if (game_ui_controller_instance == nullptr) {
        game_ui_controller_instance = std::unique_ptr<GameUIController>(new GameUIController(initialize_params));
    }
}

void GameUIController::update() {
    if (is_in_hide()) {
        if (!is_hiding_until_notice()) {
            hide_frames_passed++;
        
            if (hide_frames_passed > total_hide_frame) {
                show_impl();
            }
        }
    }
}

int GameUIController::pre_gui000020_on_gui_awake(int argc, void** argv, REFrameworkTypeDefinitionHandle* arg_tys, unsigned long long ret_addr) {
    if (game_ui_controller_instance) {
        auto gui_comp = reinterpret_cast<reframework::API::ManagedObject*>(argv[1]);
        auto game_object = gui_comp->call<reframework::API::ManagedObject*>("get_GameObject", argv[0], gui_comp);

        game_ui_controller_instance->notification_GO = game_object;
    }

    return REFRAMEWORK_HOOK_CALL_ORIGINAL;
}

void GameUIController::post_gui000020_on_gui_awake(void** ret_val, REFrameworkTypeDefinitionHandle ret_ty, unsigned long long ret_addr) {

}
