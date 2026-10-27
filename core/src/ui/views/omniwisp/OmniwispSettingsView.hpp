#pragma once

#include "OmniwispCommon.hpp"
#include <functional>

namespace cbdos {
namespace ui {

class OmniwispSettingsView {
public:
    using OnSaveSettingsCb = std::function<void(const OmniSettings& settings)>;

    static void render(lv_obj_t* parent,
                       const OmniSettings& currentSettings,
                       OnSaveSettingsCb onSave);

private:
    static lv_obj_t* s_taUser;
    static lv_obj_t* s_taPass;
    static lv_obj_t* s_taPort;
    static lv_obj_t* s_taTimeout;
};

} // namespace ui
} // namespace cbdos
