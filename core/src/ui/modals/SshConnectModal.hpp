#pragma once
#include <lvgl.h>
#include <string>
#include <vector>
#include <functional>
#include "cbdos/ssh.hpp"

namespace cbdos {
namespace ui {

struct SshHostProfile {
    std::string name;
    std::string host;
    uint16_t port{22};
    std::string username;
    cbdos::ssh::SshAuthType authType{cbdos::ssh::SshAuthType::Password};
    std::string password;
    std::string keyPath;
    std::string passphrase;
};

class SshConnectModal {
public:
    using ConnectCallback = std::function<void(const cbdos::ssh::SshConfig& config)>;

    SshConnectModal() = default;
    ~SshConnectModal() {
        close();
    }

    void show(lv_obj_t* parent, ConnectCallback onConnect);
    void close();
    void setStatus(const std::string& msg, bool isError = false);
    bool isOpen() const { return m_modalMask != nullptr; }

private:
    void loadProfiles();
    void saveCurrentProfile();
    void applySelectedProfile(size_t index);
    void updateProfileDropdown();

    static void profileChangedCb(lv_event_t* e);
    static void authTypeChangedCb(lv_event_t* e);
    static void closeBtnCb(lv_event_t* e);
    static void connectBtnCb(lv_event_t* e);
    static void saveProfileBtnCb(lv_event_t* e);
    static void togglePassVisCb(lv_event_t* e);

    lv_obj_t* m_modalMask = nullptr;
    lv_obj_t* m_card = nullptr;
    lv_obj_t* m_ddProfile = nullptr;
    lv_obj_t* m_btnSaveProfile = nullptr;
    lv_obj_t* m_taHost = nullptr;
    lv_obj_t* m_taPort = nullptr;
    lv_obj_t* m_taUser = nullptr;
    lv_obj_t* m_ddAuthType = nullptr;
    lv_obj_t* m_ddTerm = nullptr;
    
    // Contenedores según método de autenticación
    lv_obj_t* m_boxPassword = nullptr;
    lv_obj_t* m_taPassword = nullptr;
    lv_obj_t* m_btnPassVis = nullptr;

    lv_obj_t* m_boxKey = nullptr;
    lv_obj_t* m_taKeyPath = nullptr;
    lv_obj_t* m_taPassphrase = nullptr;

    lv_obj_t* m_lblStatus = nullptr;

    bool m_passVisible = false;
    std::vector<SshHostProfile> m_profiles;
    ConnectCallback m_onConnect;
};

} // namespace ui
} // namespace cbdos
