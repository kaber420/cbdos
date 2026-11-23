#pragma once
#include "cbdos/flasher.hpp"
#include <string>
#include <vector>

namespace cbdos {
namespace system {

class FlasherServiceP4 {
public:
    static FlasherServiceP4& getInstance();

    // Inicia el proceso de flasheo universal en segundo plano
    bool startFlash(const cbdos::flasher::FlasherConfig& config, cbdos::flasher::FlasherProgressCb progressCb = nullptr);
    bool startFlash(cbdos::flasher::FlasherProgressCb progressCb = nullptr);

    bool isBusy() const { return m_busy; }
    cbdos::flasher::FlasherStatus getStatus() const { return m_status; }
    int getProgress() const { return m_progress; }
    std::string getStatusMessage() const { return m_message; }

    const std::vector<cbdos::flasher::FlasherPreset>& getPresets() const;
    cbdos::flasher::FlasherConfig getDefaultConfig() const;

private:
    FlasherServiceP4();
    static void flashTaskWrapper(void* arg);
    void runFlashTask();

    bool m_busy = false;
    cbdos::flasher::FlasherConfig m_activeConfig;
    cbdos::flasher::FlasherStatus m_status = cbdos::flasher::FlasherStatus::Idle;
    int m_progress = 0;
    std::string m_message = "Listo";
    cbdos::flasher::FlasherProgressCb m_cb = nullptr;
    std::vector<cbdos::flasher::FlasherPreset> m_presets;
};

} // namespace system
} // namespace cbdos
