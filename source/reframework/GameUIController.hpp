#pragma once

#include <reframework/API.h>

class GameUIController {
private:
    int total_hide_frame{ -1 };
    int hide_frames_passed{ -1 };

    explicit GameUIController(const REFrameworkPluginInitializeParam* initialize_params);

    void show_impl();

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

    static GameUIController *get_instance();
    static void initialize(const REFrameworkPluginInitializeParam *initialize_params);
};