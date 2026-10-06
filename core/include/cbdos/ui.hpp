#pragma once
#include <cstdint>
#include <cstddef>

namespace cbdos {
namespace ui {

bool init();
void update();
void openDashboard();
void openTableHubKds();
void openTableHubTabletop();
void toggleQuickSettings();
bool isQuickSettingsOpen();
void showNotification(const char* message, uint32_t durationMs = 3000);

} // namespace ui
} // namespace cbdos
