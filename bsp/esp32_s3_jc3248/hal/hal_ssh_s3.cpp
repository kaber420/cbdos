#include "cbdos/ssh.hpp"
#include <libssh/libssh.h>
#include <Arduino.h>
#include <cstring>
#include <vector>

namespace cbdos {
namespace bsp {

class S3SshClient : public cbdos::ssh::ISshClient {
public:
    S3SshClient() : m_session(nullptr), m_shellChannel(nullptr), m_shellTaskHandle(nullptr), m_stopShell(false) {}

    ~S3SshClient() override {
        disconnect();
    }

    bool connect(const cbdos::ssh::SshConfig& config, cbdos::ssh::SshStateCallback onStateChanged) override {
        disconnect();

        if (onStateChanged) {
            onStateChanged(cbdos::ssh::SshSessionState::ConnectingTcp, "Iniciando conexión SSH...");
        }

        static bool s_sshInited = false;
        if (!s_sshInited) {
            if (ssh_init() != SSH_OK) {
                if (onStateChanged) {
                    onStateChanged(cbdos::ssh::SshSessionState::ErrorSocket, "Fallo al inicializar libssh");
                }
                return false;
            }
            s_sshInited = true;
        }

        m_session = ssh_new();
        if (!m_session) {
            if (onStateChanged) {
                onStateChanged(cbdos::ssh::SshSessionState::ErrorSocket, "Fallo al asignar memoria para sesión SSH");
            }
            return false;
        }

        ssh_options_set(m_session, SSH_OPTIONS_HOST, config.host.c_str());
        int port = config.port;
        ssh_options_set(m_session, SSH_OPTIONS_PORT, &port);
        ssh_options_set(m_session, SSH_OPTIONS_USER, config.username.c_str());
        long timeoutSec = (config.timeoutMs + 999) / 1000;
        ssh_options_set(m_session, SSH_OPTIONS_TIMEOUT, &timeoutSec);
        bool processConfig = false;
        ssh_options_set(m_session, SSH_OPTIONS_PROCESS_CONFIG, &processConfig);
        const char* sshDir = "/spiffs";
        ssh_options_set(m_session, SSH_OPTIONS_SSH_DIR, sshDir);
        const char* knownHosts = "/spiffs/known_hosts";
        ssh_options_set(m_session, SSH_OPTIONS_KNOWNHOSTS, knownHosts);

        int rc = ssh_connect(m_session);
        if (rc != SSH_OK) {
            std::string err = ssh_get_error(m_session);
            if (onStateChanged) {
                onStateChanged(cbdos::ssh::SshSessionState::ErrorSocket, err);
            }
            ssh_free(m_session);
            m_session = nullptr;
            return false;
        }

        if (onStateChanged) {
            onStateChanged(cbdos::ssh::SshSessionState::Authenticating, "Autenticando credenciales...");
        }

        if (config.authType == cbdos::ssh::SshAuthType::Password) {
            rc = ssh_userauth_password(m_session, nullptr, config.password.c_str());
            if (rc != SSH_AUTH_SUCCESS) {
                std::string err = ssh_get_error(m_session);
                if (onStateChanged) {
                    onStateChanged(cbdos::ssh::SshSessionState::ErrorAuthFailed, err);
                }
                disconnect();
                return false;
            }
        } else if (config.authType == cbdos::ssh::SshAuthType::PublicKey) {
            ssh_key privkey = nullptr;
            rc = ssh_pki_import_privkey_file(config.privateKeyPath.c_str(),
                                             config.passphrase.empty() ? nullptr : config.passphrase.c_str(),
                                             nullptr, nullptr, &privkey);
            if (rc == SSH_OK && privkey) {
                rc = ssh_userauth_publickey(m_session, nullptr, privkey);
                ssh_key_free(privkey);
                if (rc != SSH_AUTH_SUCCESS) {
                    std::string err = ssh_get_error(m_session);
                    if (onStateChanged) {
                        onStateChanged(cbdos::ssh::SshSessionState::ErrorAuthFailed, err);
                    }
                    disconnect();
                    return false;
                }
            } else {
                std::string err = "No se pudo leer clave privada desde: " + config.privateKeyPath;
                if (onStateChanged) {
                    onStateChanged(cbdos::ssh::SshSessionState::ErrorAuthFailed, err);
                }
                disconnect();
                return false;
            }
        }

        if (onStateChanged) {
            onStateChanged(cbdos::ssh::SshSessionState::Ready, "Conexión SSH establecida y autenticada");
        }
        return true;
    }

    void disconnect() override {
        closeShell();
        if (m_session) {
            ssh_disconnect(m_session);
            ssh_free(m_session);
            m_session = nullptr;
        }
    }

    bool isConnected() const override {
        return (m_session != nullptr && ssh_is_connected(m_session));
    }

    cbdos::ssh::SshExecResult execute(const std::string& command, uint32_t timeoutMs) override {
        cbdos::ssh::SshExecResult result;
        if (!isConnected()) {
            result.success = false;
            result.exitCode = -1;
            result.errorMessage = "Sesión SSH no conectada";
            return result;
        }

        ssh_channel channel = ssh_channel_new(m_session);
        if (!channel) {
            result.success = false;
            result.errorMessage = "Error al crear canal SSH: " + std::string(ssh_get_error(m_session));
            return result;
        }

        if (ssh_channel_open_session(channel) != SSH_OK) {
            result.success = false;
            result.errorMessage = "Error al abrir sesión de canal: " + std::string(ssh_get_error(m_session));
            ssh_channel_free(channel);
            return result;
        }

        if (ssh_channel_request_exec(channel, command.c_str()) != SSH_OK) {
            result.success = false;
            result.errorMessage = "Error al ejecutar comando: " + std::string(ssh_get_error(m_session));
            ssh_channel_close(channel);
            ssh_channel_free(channel);
            return result;
        }

        char buffer[256];
        int nbytes = 0;
        while ((nbytes = ssh_channel_read(channel, buffer, sizeof(buffer) - 1, 0)) > 0) {
            result.stdOut.append(buffer, nbytes);
        }

        while ((nbytes = ssh_channel_read(channel, buffer, sizeof(buffer) - 1, 1)) > 0) {
            result.stdErr.append(buffer, nbytes);
        }

        uint32_t exit_code = 0;
        if (ssh_channel_get_exit_state(channel, &exit_code, nullptr, nullptr) == SSH_OK) {
            result.exitCode = static_cast<int>(exit_code);
        } else {
            result.exitCode = 0;
        }
        result.success = (nbytes >= 0);

        ssh_channel_send_eof(channel);
        ssh_channel_close(channel);
        ssh_channel_free(channel);

        return result;
    }

    bool openShell(cbdos::ssh::SshDataCallback onData, uint16_t cols, uint16_t rows, const std::string& termType) override {
        if (!isConnected()) {
            return false;
        }

        closeShell();

        m_shellChannel = ssh_channel_new(m_session);
        if (!m_shellChannel) {
            return false;
        }

        if (ssh_channel_open_session(m_shellChannel) != SSH_OK) {
            ssh_channel_free(m_shellChannel);
            m_shellChannel = nullptr;
            return false;
        }

        const char* ptyType = termType.empty() ? "vt100" : termType.c_str();
        if (ssh_channel_request_pty_size(m_shellChannel, ptyType, cols, rows) != SSH_OK) {
            ssh_channel_close(m_shellChannel);
            ssh_channel_free(m_shellChannel);
            m_shellChannel = nullptr;
            return false;
        }

        if (ssh_channel_request_shell(m_shellChannel) != SSH_OK) {
            ssh_channel_close(m_shellChannel);
            ssh_channel_free(m_shellChannel);
            m_shellChannel = nullptr;
            return false;
        }

        m_onData = onData;
        m_stopShell = false;

        xTaskCreatePinnedToCore([](void* arg) {
            S3SshClient* self = static_cast<S3SshClient*>(arg);
            char buf[256];
            while (!self->m_stopShell && self->m_shellChannel && ssh_channel_is_open(self->m_shellChannel)) {
                int nbytes = ssh_channel_read_nonblocking(self->m_shellChannel, buf, sizeof(buf), 0);
                if (nbytes > 0 && self->m_onData) {
                    self->m_onData(reinterpret_cast<const uint8_t*>(buf), nbytes);
                } else if (nbytes < 0) {
                    break;
                }
                vTaskDelay(pdMS_TO_TICKS(15));
            }
            self->m_shellTaskHandle = nullptr;
            vTaskDelete(nullptr);
        }, "s3_ssh_rx", 4096, this, 5, &m_shellTaskHandle, 1);

        return true;
    }

    bool sendInput(const uint8_t* buffer, size_t length) override {
        if (!m_shellChannel || !ssh_channel_is_open(m_shellChannel)) {
            return false;
        }
        int written = ssh_channel_write(m_shellChannel, buffer, length);
        return (written == static_cast<int>(length));
    }

    void resizePty(uint16_t cols, uint16_t rows) override {
        if (m_shellChannel && ssh_channel_is_open(m_shellChannel)) {
            ssh_channel_change_pty_size(m_shellChannel, cols, rows);
        }
    }

    void closeShell() override {
        m_stopShell = true;
        if (m_shellTaskHandle) {
            vTaskDelay(pdMS_TO_TICKS(50));
        }
        if (m_shellChannel) {
            ssh_channel_close(m_shellChannel);
            ssh_channel_free(m_shellChannel);
            m_shellChannel = nullptr;
        }
        m_onData = nullptr;
    }

private:
    ssh_session m_session;
    ssh_channel m_shellChannel;
    cbdos::ssh::SshDataCallback m_onData;
    TaskHandle_t m_shellTaskHandle;
    volatile bool m_stopShell;
};

static S3SshClient s_s3SshClient;

void initSshBackendS3() {
    cbdos::ssh::setSshClient(&s_s3SshClient);
}

} // namespace bsp
} // namespace cbdos
