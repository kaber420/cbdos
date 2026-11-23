#include "cbdos/ssh.hpp"
#include <libssh2.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <netdb.h>
#include <fcntl.h>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <cstring>
#include <string>
#include <vector>
#include <mutex>

static const char* TAG = "HAL_SSH_P4";

namespace cbdos {
namespace bsp {

class P4SshClient : public cbdos::ssh::ISshClient {
public:
    P4SshClient()
        : m_socket(-1)
        , m_session(nullptr)
        , m_shellChannel(nullptr)
        , m_onData(nullptr)
        , m_shellTaskHandle(nullptr)
        , m_stopShell(false)
        , m_connected(false)
    {}

    ~P4SshClient() override {
        disconnect();
    }

    bool connect(const cbdos::ssh::SshConfig& config, cbdos::ssh::SshStateCallback onStateChanged) override {
        disconnect();

        if (onStateChanged) {
            onStateChanged(cbdos::ssh::SshSessionState::ConnectingTcp, "Resolviendo host y conectando socket TCP...");
        }

        struct addrinfo hints;
        memset(&hints, 0, sizeof(hints));
        hints.ai_family = AF_INET;
        hints.ai_socktype = SOCK_STREAM;

        char portStr[10];
        snprintf(portStr, sizeof(portStr), "%d", config.port);

        struct addrinfo* res = nullptr;
        int rc = getaddrinfo(config.host.c_str(), portStr, &hints, &res);
        if (rc != 0 || !res) {
            ESP_LOGE(TAG, "Fallo al resolver host %s", config.host.c_str());
            if (onStateChanged) {
                onStateChanged(cbdos::ssh::SshSessionState::ErrorSocket, "No se pudo resolver el host");
            }
            return false;
        }

        m_socket = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
        if (m_socket < 0) {
            ESP_LOGE(TAG, "Fallo al crear socket TCP");
            freeaddrinfo(res);
            if (onStateChanged) {
                onStateChanged(cbdos::ssh::SshSessionState::ErrorSocket, "Fallo al crear socket");
            }
            return false;
        }

        struct timeval tv;
        tv.tv_sec = config.timeoutMs / 1000;
        tv.tv_usec = (config.timeoutMs % 1000) * 1000;
        setsockopt(m_socket, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
        setsockopt(m_socket, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));

        ESP_LOGI(TAG, "Conectando TCP a %s:%d", config.host.c_str(), config.port);
        if (::connect(m_socket, res->ai_addr, res->ai_addrlen) < 0) {
            ESP_LOGE(TAG, "Fallo connect() TCP a %s:%d", config.host.c_str(), config.port);
            freeaddrinfo(res);
            close(m_socket);
            m_socket = -1;
            if (onStateChanged) {
                onStateChanged(cbdos::ssh::SshSessionState::ErrorSocket, "Fallo de conexión TCP");
            }
            return false;
        }
        freeaddrinfo(res);

        static bool s_libssh2Inited = false;
        if (!s_libssh2Inited) {
            libssh2_init(0);
            s_libssh2Inited = true;
        }

        m_session = libssh2_session_init();
        if (!m_session) {
            ESP_LOGE(TAG, "Fallo al inicializar sesión libssh2");
            close(m_socket);
            m_socket = -1;
            if (onStateChanged) {
                onStateChanged(cbdos::ssh::SshSessionState::ErrorSocket, "Fallo al inicializar libssh2");
            }
            return false;
        }

        libssh2_session_set_blocking(m_session, 1);

        if (onStateChanged) {
            onStateChanged(cbdos::ssh::SshSessionState::ConnectingTcp, "Iniciando handshake SSH2...");
        }

        ESP_LOGI(TAG, "Ejecutando handshake SSH2...");
        rc = libssh2_session_handshake(m_session, m_socket);
        if (rc != 0) {
            char* errMsg = nullptr;
            libssh2_session_last_error(m_session, &errMsg, nullptr, 0);
            std::string errStr = errMsg ? errMsg : "Fallo en handshake SSH2";
            ESP_LOGE(TAG, "Error en libssh2_session_handshake: %s (%d)", errStr.c_str(), rc);
            if (onStateChanged) {
                onStateChanged(cbdos::ssh::SshSessionState::ErrorSocket, errStr);
            }
            disconnect();
            return false;
        }

        if (onStateChanged) {
            onStateChanged(cbdos::ssh::SshSessionState::Authenticating, "Autenticando credenciales...");
        }

        if (config.authType == cbdos::ssh::SshAuthType::Password) {
            ESP_LOGI(TAG, "Autenticando usuario '%s' por contraseña", config.username.c_str());
            rc = libssh2_userauth_password(m_session, config.username.c_str(), config.password.c_str());
            if (rc != 0) {
                char* errMsg = nullptr;
                libssh2_session_last_error(m_session, &errMsg, nullptr, 0);
                std::string errStr = errMsg ? errMsg : "Fallo autenticación contraseña";
                ESP_LOGE(TAG, "Fallo autenticación por contraseña: %s (%d)", errStr.c_str(), rc);
                if (onStateChanged) {
                    onStateChanged(cbdos::ssh::SshSessionState::ErrorAuthFailed, errStr);
                }
                disconnect();
                return false;
            }
        } else if (config.authType == cbdos::ssh::SshAuthType::PublicKey) {
            std::string pubKeyPath = config.privateKeyPath + ".pub";
            ESP_LOGI(TAG, "Autenticando usuario '%s' con clave privada: %s", config.username.c_str(), config.privateKeyPath.c_str());
            rc = libssh2_userauth_publickey_fromfile(m_session, config.username.c_str(),
                                                     pubKeyPath.c_str(),
                                                     config.privateKeyPath.c_str(),
                                                     config.passphrase.empty() ? nullptr : config.passphrase.c_str());
            if (rc != 0) {
                char* errMsg = nullptr;
                libssh2_session_last_error(m_session, &errMsg, nullptr, 0);
                std::string errStr = errMsg ? errMsg : "Fallo autenticación clave pública";
                ESP_LOGE(TAG, "Fallo autenticación clave pública: %s (%d)", errStr.c_str(), rc);
                if (onStateChanged) {
                    onStateChanged(cbdos::ssh::SshSessionState::ErrorAuthFailed, errStr);
                }
                disconnect();
                return false;
            }
        }

        m_connected = true;
        ESP_LOGI(TAG, "Sesión SSH2 autenticada con éxito con %s", config.host.c_str());
        if (onStateChanged) {
            onStateChanged(cbdos::ssh::SshSessionState::Ready, "Conexión SSH establecida y autenticada");
        }
        return true;
    }

    void disconnect() override {
        closeShell();
        m_connected = false;
        std::lock_guard<std::mutex> lock(m_sessionMutex);
        if (m_session) {
            ESP_LOGI(TAG, "Cerrando sesión SSH2");
            libssh2_session_disconnect(m_session, "Normal Shutdown");
            libssh2_session_free(m_session);
            m_session = nullptr;
        }
        if (m_socket >= 0) {
            close(m_socket);
            m_socket = -1;
        }
    }

    bool isConnected() const override {
        return (m_connected && m_session != nullptr && m_socket >= 0);
    }

    cbdos::ssh::SshExecResult execute(const std::string& command, uint32_t timeoutMs) override {
        cbdos::ssh::SshExecResult result;
        if (!isConnected()) {
            result.success = false;
            result.exitCode = -1;
            result.errorMessage = "Sesión SSH no conectada";
            return result;
        }

        std::lock_guard<std::mutex> lock(m_sessionMutex);
        libssh2_session_set_blocking(m_session, 1);
        LIBSSH2_CHANNEL* channel = libssh2_channel_open_session(m_session);
        if (!channel) {
            result.success = false;
            result.errorMessage = "Error al abrir canal SSH";
            return result;
        }

        int rc = libssh2_channel_exec(channel, command.c_str());
        if (rc != 0) {
            result.success = false;
            result.errorMessage = "Error al ejecutar comando remoto";
            libssh2_channel_free(channel);
            return result;
        }

        char buffer[256];
        ssize_t nbytes = 0;
        while ((nbytes = libssh2_channel_read(channel, buffer, sizeof(buffer) - 1)) > 0) {
            result.stdOut.append(buffer, nbytes);
        }

        while ((nbytes = libssh2_channel_read_stderr(channel, buffer, sizeof(buffer) - 1)) > 0) {
            result.stdErr.append(buffer, nbytes);
        }

        result.exitCode = libssh2_channel_get_exit_status(channel);
        result.success = true;

        libssh2_channel_send_eof(channel);
        libssh2_channel_close(channel);
        libssh2_channel_wait_closed(channel);
        libssh2_channel_free(channel);

        return result;
    }

    bool openShell(cbdos::ssh::SshDataCallback onData, uint16_t cols, uint16_t rows, const std::string& termType) override {
        if (!isConnected()) {
            return false;
        }

        closeShell();

        std::lock_guard<std::mutex> lock(m_sessionMutex);
        libssh2_session_set_blocking(m_session, 1);
        m_shellChannel = libssh2_channel_open_session(m_session);
        if (!m_shellChannel) {
            ESP_LOGE(TAG, "Fallo al abrir canal de shell");
            return false;
        }

        const char* chosenTerm = termType.empty() ? "vt100" : termType.c_str();
        ESP_LOGI(TAG, "Solicitando PTY tipo '%s' (%dx%d)...", chosenTerm, cols, rows);
        if (libssh2_channel_request_pty_ex(m_shellChannel, chosenTerm, strlen(chosenTerm),
                                           nullptr, 0, cols, rows, 0, 0) != 0) {
            ESP_LOGW(TAG, "Fallo PTY '%s', reintentando con 'vt100'...", chosenTerm);
            if (libssh2_channel_request_pty_ex(m_shellChannel, "vt100", 5, nullptr, 0, cols, rows, 0, 0) != 0) {
                ESP_LOGE(TAG, "Fallo definitivo al solicitar PTY");
                libssh2_channel_free(m_shellChannel);
                m_shellChannel = nullptr;
                return false;
            }
        }

        if (libssh2_channel_shell(m_shellChannel) != 0) {
            ESP_LOGE(TAG, "Fallo al iniciar shell interactivo");
            libssh2_channel_free(m_shellChannel);
            m_shellChannel = nullptr;
            return false;
        }

        // Modo no bloqueante para lectura asíncrona segura en Core 0
        libssh2_session_set_blocking(m_session, 0);

        m_onData = onData;
        m_stopShell = false;

        // Tarea anclada a Core 0 (dejando Core 1 exclusivamente libre para LVGL):
        // Lee buffer interno primero; si está vacío, espera paquetes TCP con select()
        xTaskCreatePinnedToCore([](void* arg) {
            P4SshClient* self = static_cast<P4SshClient*>(arg);
            char buf[512];
            while (!self->m_stopShell && self->m_socket >= 0 && self->m_shellChannel) {
                ssize_t nbytes = 0;
                {
                    std::lock_guard<std::mutex> lock(self->m_sessionMutex);
                    if (self->m_shellChannel) {
                        nbytes = libssh2_channel_read(self->m_shellChannel, buf, sizeof(buf));
                    }
                }

                if (nbytes > 0) {
                    if (self->m_onData) {
                        self->m_onData(reinterpret_cast<const uint8_t*>(buf), nbytes);
                    }
                    taskYIELD();
                    continue;
                }

                if (nbytes < 0 && nbytes != LIBSSH2_ERROR_EAGAIN) {
                    ESP_LOGW(TAG, "Canal SSH cerrado o error: %d", (int)nbytes);
                    break;
                }

                // Esperar reactivamente por paquetes de red en Core 0 (cero consumo de CPU)
                fd_set rfds;
                FD_ZERO(&rfds);
                FD_SET(self->m_socket, &rfds);

                struct timeval tv;
                tv.tv_sec = 0;
                tv.tv_usec = 40000; // 40 ms para revisar bandera de parada

                select(self->m_socket + 1, &rfds, NULL, NULL, &tv);
            }
            self->m_shellTaskHandle = nullptr;
            vTaskDelete(nullptr);
        }, "ssh_shell_rx", 4096, this, 5, &m_shellTaskHandle, 0);

        return true;
    }

    bool sendInput(const uint8_t* buffer, size_t length) override {
        if (!m_shellChannel || !buffer || length == 0) {
            return false;
        }

        std::lock_guard<std::mutex> lock(m_sessionMutex);
        size_t totalWritten = 0;
        int retries = 0;
        while (totalWritten < length && retries < 50) {
            ssize_t written = libssh2_channel_write(m_shellChannel,
                                                    reinterpret_cast<const char*>(buffer + totalWritten),
                                                    length - totalWritten);
            if (written > 0) {
                totalWritten += written;
                retries = 0;
            } else if (written == LIBSSH2_ERROR_EAGAIN) {
                retries++;
                vTaskDelay(pdMS_TO_TICKS(5));
            } else {
                ESP_LOGE(TAG, "Fallo al escribir en canal SSH: %d", (int)written);
                return false;
            }
        }
        return (totalWritten == length);
    }

    void resizePty(uint16_t cols, uint16_t rows) override {
        std::lock_guard<std::mutex> lock(m_sessionMutex);
        if (m_shellChannel) {
            libssh2_channel_request_pty_size_ex(m_shellChannel, cols, rows, 0, 0);
        }
    }

    void closeShell() override {
        m_stopShell = true;
        if (m_shellTaskHandle) {
            vTaskDelay(pdMS_TO_TICKS(60));
        }
        std::lock_guard<std::mutex> lock(m_sessionMutex);
        if (m_shellChannel) {
            libssh2_session_set_blocking(m_session, 1);
            libssh2_channel_send_eof(m_shellChannel);
            libssh2_channel_close(m_shellChannel);
            libssh2_channel_wait_closed(m_shellChannel);
            libssh2_channel_free(m_shellChannel);
            m_shellChannel = nullptr;
        }
        m_onData = nullptr;
    }

private:
    int m_socket;
    LIBSSH2_SESSION* m_session;
    LIBSSH2_CHANNEL* m_shellChannel;
    cbdos::ssh::SshDataCallback m_onData;
    TaskHandle_t m_shellTaskHandle;
    volatile bool m_stopShell;
    bool m_connected;
    std::mutex m_sessionMutex;
};

static P4SshClient s_p4SshClient;

void initSshBackendP4() {
    cbdos::ssh::setSshClient(&s_p4SshClient);
    ESP_LOGI(TAG, "HAL SSH2 P4 inicializado");
}

} // namespace bsp
} // namespace cbdos
