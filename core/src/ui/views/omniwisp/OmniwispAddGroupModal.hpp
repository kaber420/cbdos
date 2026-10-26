#pragma once

#include "OmniwispCommon.hpp"
#include <functional>

namespace cbdos {
namespace ui {

class OmniwispAddGroupModal {
public:
    using OnGroupSavedCb = std::function<void(const cbdos::omniwisp::NetworkGroup& group)>;

    static void show(lv_obj_t* parent, const std::string& parentId, OnGroupSavedCb onSave);
    static void close();

private:
    static lv_obj_t* s_mask;
    static lv_obj_t* s_card;
    static lv_obj_t* s_taName;
    static lv_obj_t* s_taNotes;
    static std::string s_parentId;
    static OnGroupSavedCb s_onSave;
};

} // namespace ui
} // namespace cbdos
