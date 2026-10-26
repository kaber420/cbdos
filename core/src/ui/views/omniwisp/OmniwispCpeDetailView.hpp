#pragma once

#include "OmniwispCommon.hpp"
#include <functional>

namespace cbdos {
namespace ui {

class OmniwispCpeDetailView {
public:
    using OnBackCb = std::function<void()>;

    static void render(lv_obj_t* parent, const cbdos::network::AirOSStation& cpe, OnBackCb onBack);
};

} // namespace ui
} // namespace cbdos
