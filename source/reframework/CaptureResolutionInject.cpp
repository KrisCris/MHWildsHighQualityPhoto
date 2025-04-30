#include "CaptureResolutionInject.hpp"
#include "REFrameworkBorrowedAPI.hpp"
#include <numeric>

static const char *HIGH_RESOLUTION_CAPTURE_PACK_AVAILABLE_FILE = "reframework/data/MHWildsHQQuestResult_HighResolutionCapturePackAvailable";

static std::map<std::pair<int, int>, std::string> paths_to_capture_render_target_16x9 = {
    {{1920, 1080}, "RenderTexture/16x9/Quest_Background_16x9_1080p.rtex"},
    {{2560, 1440}, "RenderTexture/16x9/Quest_Background_16x9_2K.rtex"},
    {{3840, 2160}, "RenderTexture/16x9/Quest_Background_16x9_4K.rtex"},
};

static std::map<std::pair<int, int>, std::string> paths_to_capture_render_target_21x9 = {
    {{2560, 1080}, "RenderTexture/21x9/Quest_Background_21x9_1080p.rtex"},
    {{3440, 1440}, "RenderTexture/21x9/Quest_Background_21x9_3440x1440.rtex"},
    {{5120, 2160}, "RenderTexture/21x9/Quest_Background_21x9_4K.rtex"},
};

static const char *RENDER_TARGET_TEXTURE_TYPE_NAME = "via.render.RenderTargetTextureResource";
static const char *RENDER_TARGET_TEXTURE_HOLDER_TYPE_NAME = "via.render.RenderTargetTextureResourceHolder";

static bool is_high_res_capture_pack_available() {
    auto persistent_dir = REFramework::get_persistent_dir();
    auto path_enable = persistent_dir / HIGH_RESOLUTION_CAPTURE_PACK_AVAILABLE_FILE;

    return std::filesystem::exists(path_enable);
}

std::unique_ptr<CaptureResolutionInject> capture_resolution_inject_instance = nullptr;

CaptureResolutionInject::Resolution DEFAULT_RESOLUTION_16x9 = {1920, 1080};
CaptureResolutionInject::Resolution DEFAULT_RESOLUTION_21x9 = {2560, 1080};

void set_render_target_texture_resource(reframework::API::ManagedObject *holder, reframework::API::Resource *resource) {
    *reinterpret_cast<reframework::API::Resource**>(reinterpret_cast<uintptr_t>(holder) + 0x10) = resource;
}

reframework::API::Resource *get_render_target_texture_resource(reframework::API::ManagedObject *holder) {
    return *reinterpret_cast<reframework::API::Resource**>(reinterpret_cast<uintptr_t>(holder) + 0x10);
}

static reframework::API::ManagedObject *create_render_target_texture_resource_holder(reframework::API *api, reframework::API::Resource *resource) {
    auto tdb = api->tdb();
    auto render_target_texture_holder_type = tdb->find_type(RENDER_TARGET_TEXTURE_HOLDER_TYPE_NAME);
    auto render_target_texture_holder = render_target_texture_holder_type->create_instance();

    if (!render_target_texture_holder) {
        api->log_error("Failed to create RenderTargetTextureHolder instance");
        return nullptr;
    }

    resource->add_ref();
    set_render_target_texture_resource(render_target_texture_holder, resource);

    return render_target_texture_holder;
}

CaptureResolutionInject::CaptureResolutionInject(reframework::API* api_instance) : api(api_instance) {
    enabled = is_high_res_capture_pack_available();

    if (!enabled) {
        api->log_warn("Quest result background: HD capture pack is not available");
        return;
    } else {
        api->log_info("Quest result background: HD capture pack is available");
    }

    auto resource_manager = api->resource_manager();

    for (const auto& [resolution, path] : paths_to_capture_render_target_16x9) {
        auto resource = resource_manager->create_resource(RENDER_TARGET_TEXTURE_TYPE_NAME, path);
        if (resource) {
            api->log_error("Loaded render target with resolution %dx%d for 16:9 monitor", resolution.first, resolution.second);

            resource->add_ref();

            ResourceInfo resource_info;
            resource_info.resource = resource;
            resource_info.holder = create_render_target_texture_resource_holder(api, resource);
            resource_info.is_plugin_managed = true;

            resource_info.holder->add_ref();

            capture_render_targets_by_resolution_16x9[resolution] = resource_info;
        } else {
            api->log_error("Failed to load render target for resolution %dx%d", resolution.first, resolution.second);
        }
    }

    for (const auto& [resolution, path] : paths_to_capture_render_target_21x9) {
        auto resource = resource_manager->create_resource(RENDER_TARGET_TEXTURE_TYPE_NAME, path);
        if (resource) {
            api->log_error("Loaded render target with resolution %dx%d for 21:9 monitor", resolution.first, resolution.second);

            resource->add_ref();

            ResourceInfo resource_info;
            resource_info.resource = resource;
            resource_info.holder = create_render_target_texture_resource_holder(api, resource);
            resource_info.is_plugin_managed = true;

            resource_info.holder->add_ref();

            capture_render_targets_by_resolution_21x9[resolution] = resource_info;
        } else {
            api->log_error("Failed to load render target for resolution %dx%d", resolution.first, resolution.second);
        }
    }

    album_manager = api->get_managed_singleton("app.AlbumManager");

    if (!album_manager) {
        api->log_error("Failed to get AlbumManager singleton");
    }

    render_target_texture_holder_field_16x9 = album_manager->get_field<reframework::API::ManagedObject*>("_RTT_16x9");
    render_target_texture_holder_field_21x9 = album_manager->get_field<reframework::API::ManagedObject*>("_RTT_21x9");

    // NOTE: Debugging, so log out the part where I add game-created render target, use mine instead
    if (render_target_texture_holder_field_16x9) {
        auto resource_16x9_default = *render_target_texture_holder_field_16x9;
        if (resource_16x9_default) {
            ResourceInfo resource_info;
            resource_info.resource = get_render_target_texture_resource(resource_16x9_default);
            resource_info.holder = resource_16x9_default;
            resource_info.is_plugin_managed = false;

            //capture_render_targets_by_resolution_16x9[DEFAULT_RESOLUTION_16x9] = resource_info;

            default_16x9_render_target_texture_holder = resource_info.holder;
            default_16x9_render_target_texture_holder->add_ref();
        } else {
            api->log_error("Failed to get default render target for 16x9 resolution. Field value is null!");
        }
    } else {
        api->log_error("Failed to get default render target for 16x9 resolution");
    }

    if (render_target_texture_holder_field_21x9) {
        auto resource_21x9_default = *render_target_texture_holder_field_21x9;
        if (resource_21x9_default) {
            ResourceInfo resource_info;
            resource_info.resource = get_render_target_texture_resource(resource_21x9_default);
            resource_info.holder = resource_21x9_default;
            resource_info.is_plugin_managed = false;

            //capture_render_targets_by_resolution_21x9[DEFAULT_RESOLUTION_21x9] = resource_info;
            default_21x9_render_target_texture_holder = resource_info.holder;

            default_21x9_render_target_texture_holder->add_ref();
        } else {
            api->log_error("Failed to get default render target for 21x9 resolution. Field value is null!");
        }
    } else {
        api->log_error("Failed to get default render target for 21x9 resolution");
    }
    
    auto tdb = api->tdb();
    auto set_option_method = tdb->find_method("app.cGUISystemModuleOption", "onLateUpdate");

    if (set_option_method == nullptr) {
        api->log_error("Failed to find onLateUpdate method in app.cGUISystemModuleOption");
        return;
    }
    else {
        set_option_method->add_hook(pre_gui_system_module_option_on_late_update, post_gui_system_module_option_on_late_update, false);
    }

    get_resolution_method = tdb->find_method("app.cGUISystemModuleOption", "getCurrentResolution");
    is_widescreen_method = tdb->find_method("app.OptionUtil", "isUltraWide");
    set_texture_method = tdb->find_method("via.gui.Texture", "setTexture");

    current_resolution_16x9 = DEFAULT_RESOLUTION_16x9;
    current_resolution_21x9 = DEFAULT_RESOLUTION_21x9;

    auto on_open_gui_quest_result_method = tdb->find_method("app.GUI070002", "onOpen");
    if (on_open_gui_quest_result_method == nullptr) {
        api->log_error("Failed to find onOpen method in app.GUI070002");
    } else {
        on_open_gui_quest_result_method->add_hook(pre_gui070002_on_open, post_gui070002_on_open, false);
    }

    auto on_close_gui_quest_result_method = tdb->find_method("app.GUI070002", "onClose");
    if (on_close_gui_quest_result_method == nullptr) {
        api->log_error("Failed to find onClose method in app.GUI070002");
    } else {
        on_close_gui_quest_result_method->add_hook(pre_gui070002_on_close, post_gui070002_on_close, false);
    }
}

int CaptureResolutionInject::pre_gui_system_module_option_on_late_update(int argc, void** argv, REFrameworkTypeDefinitionHandle* arg_tys, unsigned long long ret_addr) {
    if (capture_resolution_inject_instance) {
        capture_resolution_inject_instance->system_module_option_obj = reinterpret_cast<reframework::API::ManagedObject*>(argv[1]);
    }

    return REFRAMEWORK_HOOK_CALL_ORIGINAL;
}

void CaptureResolutionInject::post_gui_system_module_option_on_late_update(void** ret_val, REFrameworkTypeDefinitionHandle ret_ty, unsigned long long ret_addr) {
}

int CaptureResolutionInject::pre_gui070002_on_open(int argc, void** argv, REFrameworkTypeDefinitionHandle* arg_tys, unsigned long long ret_addr) {
    if (capture_resolution_inject_instance) {
        capture_resolution_inject_instance->gui_quest_instance = reinterpret_cast<reframework::API::ManagedObject*>(argv[1]);
    }

    return REFRAMEWORK_HOOK_CALL_ORIGINAL;
}

void CaptureResolutionInject::post_gui070002_on_open(void** ret_val, REFrameworkTypeDefinitionHandle ret_ty, unsigned long long ret_addr) {
    if (capture_resolution_inject_instance) {
        capture_resolution_inject_instance->update_gui_texture();
    }
}

int CaptureResolutionInject::pre_gui070002_on_close(int argc, void** argv, REFrameworkTypeDefinitionHandle* arg_tys, unsigned long long ret_addr) {
    auto vm_this = reinterpret_cast<reframework::API::ManagedObject*>(argv[1]);

    if (capture_resolution_inject_instance->gui_quest_instance != nullptr && capture_resolution_inject_instance->gui_quest_instance == vm_this) {
        capture_resolution_inject_instance->api->log_info("Quest result GUI is closing, reverting render target changes");
        capture_resolution_inject_instance->is_gui_quest_closed = true;
    }
    
    return REFRAMEWORK_HOOK_CALL_ORIGINAL;
}

void CaptureResolutionInject::post_gui070002_on_close(void** ret_val, REFrameworkTypeDefinitionHandle ret_ty, unsigned long long ret_addr) {
    if (capture_resolution_inject_instance && capture_resolution_inject_instance->is_gui_quest_closed) {
        capture_resolution_inject_instance->revert();
        capture_resolution_inject_instance->is_gui_quest_closed = false;
        capture_resolution_inject_instance->gui_quest_instance = nullptr;
    }
}

reframework::API::ManagedObject *CaptureResolutionInject::find_nearest_render_target_texture_holder(float current_width, float current_height, RenderTargetMap& render_target_map,
    Resolution *output_resolution) const {
    float epsilon = std::numeric_limits<float>::epsilon();

    float best_difference = std::numeric_limits<float>::max();
    bool found_one = false;

    reframework::API::ManagedObject *best_render_target_holder = nullptr;
    std::pair<int, int> best_resolution = {0, 0};

    for (const auto& [resolution, resource] : render_target_map) {
        float width = static_cast<float>(resolution.first);
        float height = static_cast<float>(resolution.second);

        if (current_width < width && current_height < height) {
            // Pick this immediately
            best_render_target_holder = resource.holder;
            best_resolution = resolution;

            found_one = true;

            break;
        }

        if (std::abs(current_width - width) < epsilon && std::abs(current_height - height) < epsilon) {
            best_render_target_holder = resource.holder;
            best_resolution = resolution;

            found_one = true;

            break;
        }

        float current_difference = std::abs(current_width - width) + std::abs(current_height - height);
        if (current_difference < best_difference) {
            best_difference = current_difference;
            best_render_target_holder = resource.holder;
            best_resolution = resolution;

            found_one = true;
        }
    }

    if (found_one) {
        api->log_info("Best render target found for resolution %dx%d", best_resolution.first, best_resolution.second);
        
        if (output_resolution) {
            *output_resolution = best_resolution;
        }

        return best_render_target_holder;
    } else {
        api->log_error("No suitable render target found for resolution %dx%d", static_cast<int>(current_width), static_cast<int>(current_height));
    
        return nullptr;
    }
}

void CaptureResolutionInject::update_resolution() {
    if (!enabled) {
        return;
    }

    if (!album_manager) {
        return;
    }

    if (!get_resolution_method) {
        api->log_error("getResolutionMethod is null");
        return;
    }

    if (capture_render_targets_by_resolution_16x9.empty() && capture_render_targets_by_resolution_21x9.empty()) {
        api->log_error("No extra render targets, assuming render target PAK is not installed!");
        return;
    }

    float width_and_height[2];
    get_resolution_method->call(width_and_height, api->get_vm_context(), system_module_option_obj);

    bool is_widescreen = is_widescreen_method->call<bool>(api->get_vm_context(), nullptr);

    reframework::API::ManagedObject *best_resource_holder = nullptr;

    if (is_widescreen) {
        auto best_21x9_render_target = find_nearest_render_target_texture_holder(width_and_height[0], width_and_height[1], capture_render_targets_by_resolution_21x9,
            &current_resolution_21x9);

        if (!best_21x9_render_target) {
            api->log_error("Failed to find suitable render target for 21x9 resolution, use default");
            best_21x9_render_target = default_21x9_render_target_texture_holder;
            current_resolution_21x9 = DEFAULT_RESOLUTION_21x9;
        }

        best_resource_holder = best_21x9_render_target;

        *render_target_texture_holder_field_21x9 = best_resource_holder;
    } else {
        auto best_16x9_render_target = find_nearest_render_target_texture_holder(width_and_height[0], width_and_height[1], capture_render_targets_by_resolution_16x9,
            &current_resolution_16x9);

        if (!best_16x9_render_target) {
            api->log_error("Failed to find suitable render target for 16x9 resolution, use default");
            best_16x9_render_target = default_16x9_render_target_texture_holder;
            current_resolution_16x9 = DEFAULT_RESOLUTION_16x9;
        }

        best_resource_holder = best_16x9_render_target;
        
        *render_target_texture_holder_field_16x9 = best_resource_holder;
    }
}

void CaptureResolutionInject::cleanup() {
    for (auto& [resolution, resource] : capture_render_targets_by_resolution_16x9) {
        resource.holder->release();
        
        if (resource.is_plugin_managed) {
            resource.resource->release();
        }
    }

    for (auto& [resolution, resource] : capture_render_targets_by_resolution_21x9) {
        resource.holder->release();

        if (resource.is_plugin_managed) {
            resource.resource->release();
        }
    }
}

void CaptureResolutionInject::update_gui_texture() {
    if (!enabled) {
        return;
    }

    if (!gui_quest_instance) {
        api->log_error("GUI quest instance is null, can't update texture");
        return;
    }

    auto vm_context = api->get_vm_context();

    // Change texture. Absolutely diarrhea
    auto gui_texture_16x9_ptr = gui_quest_instance->get_field<reframework::API::ManagedObject*>("_Texture16x9");
    auto gui_texture_21x9_ptr = gui_quest_instance->get_field<reframework::API::ManagedObject*>("_Texture21x9");

    reframework::API::ManagedObject *gui_texture_16x9 = nullptr;
    reframework::API::ManagedObject *gui_texture_21x9 = nullptr;

    if (gui_texture_16x9_ptr) {
        gui_texture_16x9 = *gui_texture_16x9_ptr;
    } else {
        api->log_error("Failed to get GUI texture 16x9 field value");
    }

    if (gui_texture_21x9_ptr) {
        gui_texture_21x9 = *gui_texture_21x9_ptr;
    } else {
        api->log_error("Failed to get GUI texture 21x9 field value");
    }

    if (gui_texture_21x9) {
        set_texture_method->call(vm_context, gui_texture_21x9, *render_target_texture_holder_field_21x9);
    }

    if (gui_texture_16x9) {
        set_texture_method->call(vm_context, gui_texture_16x9, *render_target_texture_holder_field_16x9);
    }
}

void CaptureResolutionInject::revert() {
    if (!enabled) {
        return;
    }

    if (*render_target_texture_holder_field_16x9) {
        *render_target_texture_holder_field_16x9 = default_16x9_render_target_texture_holder;
    }

    if (*render_target_texture_holder_field_21x9) {
        *render_target_texture_holder_field_21x9 = default_21x9_render_target_texture_holder;
    }
}

void CaptureResolutionInject::initialize(reframework::API* api_instance) {
    if (capture_resolution_inject_instance == nullptr) {
        capture_resolution_inject_instance = std::make_unique<CaptureResolutionInject>(api_instance);
    }
}

CaptureResolutionInject* CaptureResolutionInject::get_instance() {
    return capture_resolution_inject_instance.get();
}