#pragma once

#include "OmniwispCommon.hpp"
#include <functional>

namespace cbdos {
namespace ui {

class OmniwispSectorDetailView {
public:
    using OnBackCb = std::function<void()>;
    using OnSelectCpeCb = std::function<void(const cbdos::network::AirOSStation& cpe)>;

    static void render(lv_obj_t* parent,
                      const cbdos::omniwisp::SectorDevice& sector,
                      const cbdos::network::AirOSTelemetry& telemetry,
                      bool isFetching,
                      OnBackCb onBack,
                      OnBackCb onRefresh,
                      OnSelectCpeCb onSelectCpe);
};

} // namespace ui
} // namespace cbdos
