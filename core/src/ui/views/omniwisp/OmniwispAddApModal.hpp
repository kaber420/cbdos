#pragma once

#include "OmniwispCommon.hpp"
#include <functional>

namespace cbdos {
namespace ui {

class OmniwispAddApModal {
public:
    using OnSectorSavedCb = std::function<void(const cbdos::omniwisp::SectorDevice& dev)>;

    static void show(lv_obj_t* parent, const std::string& defaultGroupId, OnSectorSavedCb onSave);
    static void close();

private:
    static lv_obj_t* s_mask;
    static lv_obj_t* s_card;
    static lv_obj_t* s_taName;
    static lv_obj_t* s_taHost;
    static lv_obj_t* s_taUser;
    static lv_obj_t* s_taPass;
    static std::string s_groupId;
    static OnSectorSavedCb s_onSave;
};

} // namespace ui
} // namespace cbdos
