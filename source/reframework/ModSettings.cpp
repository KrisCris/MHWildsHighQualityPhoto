#include "ModSettings.hpp"
#include <memory>
#include <fstream>
#include <glaze/glaze.hpp>

#include "REFrameworkBorrowedAPI.hpp"
#include <reframework/API.hpp>

static const std::string JSON_FILE_NAME = "mhwilds_high_quality_photos.json";
static const std::string DATA_FOLDER = "reframework/data";

std::unique_ptr<ModSettings> mod_settings_instance = nullptr;

static std::filesystem::path get_settings_path() {
    auto persistent_dir = REFramework::get_persistent_dir();
    return persistent_dir / DATA_FOLDER / JSON_FILE_NAME;
}

void ModSettings::initialize() {
    if (mod_settings_instance == nullptr) {
        mod_settings_instance = std::make_unique<ModSettings>();
        mod_settings_instance->load();
    }
}

ModSettings* ModSettings::get_instance() {
    return mod_settings_instance ? mod_settings_instance.get() : nullptr;
}

void ModSettings::load() {
    if (!std::filesystem::exists(get_settings_path())) {
        return;
    }

    auto& api = reframework::API::get();
    auto error_context = glz::read_file_json(*this, get_settings_path().string().c_str(), std::string{});

    if (error_context) {
        api->log_info("Failed to load settings: %s", glz::format_error(error_context));
    } else {
        api->log_info("Settings loaded successfully from %s", get_settings_path().string());
    }
}

void ModSettings::save() {
    if (!std::filesystem::exists(get_settings_path().parent_path())) {
        std::filesystem::create_directories(get_settings_path().parent_path());
    }

    auto error_context = glz::write_file_json(*this, get_settings_path().string().c_str(), std::string{});
    if (error_context) {
        auto error_str = glz::format_error(error_context);
        reframework::API::get()->log_info("Failed to save settings: %s", error_str.c_str());
    } else {
        auto settings_path = get_settings_path().string();
        reframework::API::get()->log_info("Settings saved successfully to %s", settings_path.c_str());
    }
}