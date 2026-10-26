#pragma once

#include "OmniwispCommon.hpp"
#include <functional>

namespace cbdos {
namespace ui {

class OmniwispMainView {
public:
    using OnSelectSectorCb = std::function<void(const cbdos::omniwisp::SectorDevice& sec)>;
    using OnAddGroupRequestCb = std::function<void(const std::string& parentId)>;
    using OnAddSectorRequestCb = std::function<void(const std::string& defaultGroupId)>;

    static void render(lv_obj_t* parent,
                      const std::vector<cbdos::omniwisp::NetworkGroup>& groups,
                      const std::vector<cbdos::omniwisp::SectorDevice>& sectors,
                      const std::string& activeGroupId,
                      std::function<void(const std::string& newGroupId)> onGroupFilterChanged,
                      OnSelectSectorCb onSelectSector,
                      OnAddGroupRequestCb onAddGroup,
                      OnAddSectorRequestCb onAddSector);
};

} // namespace ui
} // namespace cbdos
