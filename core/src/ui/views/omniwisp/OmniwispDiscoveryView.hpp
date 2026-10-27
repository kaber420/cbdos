#pragma once

#include "OmniwispCommon.hpp"
#include "cbdos/neighbor_discovery.hpp"
#include <functional>
#include <vector>

namespace cbdos {
namespace ui {

class OmniwispDiscoveryView {
public:
    using OnConnectDeviceCb = std::function<void(const std::string& host, const std::string& name)>;
    using OnAddDeviceCb = std::function<void(const std::string& host, const std::string& name)>;

    static void render(lv_obj_t* parent,
                       OnConnectDeviceCb onConnect,
                       OnAddDeviceCb onAdd);

    static void refreshList();
    static void updateStatus();

private:
    static lv_obj_t* s_listContainer;
    static lv_obj_t* s_lblStatus;
    static OnConnectDeviceCb s_onConnect;
    static OnAddDeviceCb s_onAdd;
};

} // namespace ui
} // namespace cbdos
