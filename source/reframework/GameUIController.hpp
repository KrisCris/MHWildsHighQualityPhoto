#pragma once

#include <reframework/API.h>
#include <reframework/API.hpp>

class GameUIController {
private:
    int total_hide_frame{ -1 };
    int hide_frames_passed{ -1 };

    bool is_in_quest_result;

    reframework::API::ManagedObject *notification_GO;

    explicit GameUIController(const REFrameworkPluginInitializeParam* initialize_params);

    void show_impl();

private:
    static int pre_gui000020_on_gui_awake(int argc, void** argv, REFrameworkTypeDefinitionHandle* arg_tys, unsigned long long ret_addr);
    static void post_gui000020_on_gui_awake(void** ret_val, REFrameworkTypeDefinitionHandle ret_ty, unsigned long long ret_addr);

public:
    ~GameUIController() = default;

    void hide_for(int frame_count);
    void update();

    float get_hiding_progress() const;

    bool is_in_hide() const {
        return hide_frames_passed >= 0;
    }

    bool is_hiding_until_notice() const {
        return total_hide_frame < 0;
    }

    void set_is_in_quest_result(bool should) {
        is_in_quest_result = should;
    }

    bool get_is_in_quest_result() {
        return is_in_quest_result;
    }

    reframework::API::ManagedObject *get_notification_gameobject() {
        return notification_GO;
    }

    static GameUIController *get_instance();
    static void initialize(const REFrameworkPluginInitializeParam *initialize_params);
};