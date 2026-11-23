if(NOT CBDOS_BOARD)
    if(DEFINED ENV{CBDOS_BOARD} AND NOT "$ENV{CBDOS_BOARD}" STREQUAL "")
        set(CBDOS_BOARD "$ENV{CBDOS_BOARD}")
    else()
        set(CBDOS_BOARD "jc1060p470")
    endif()
endif()

set(_dt_json "${CMAKE_CURRENT_LIST_DIR}/../../../boards/${CBDOS_BOARD}.json")
if(NOT EXISTS "${_dt_json}")
    message(FATAL_ERROR "[CBDos-P4-v3] No existe el archivo de placa: ${_dt_json}")
endif()

set(_dt_gen_dir "${CMAKE_CURRENT_BINARY_DIR}/generated")
set(_dt_header "${_dt_gen_dir}/cbdos_device_tree.h")
set(_panel_select_header "${_dt_gen_dir}/cbdos_panel_select.h")

file(READ "${_dt_json}" _dt_json_str)
string(JSON _panel_header ERROR_VARIABLE _json_err GET "${_dt_json_str}" "devices" "display" "panel_header")
file(MAKE_DIRECTORY "${_dt_gen_dir}")
if(_panel_header AND NOT _json_err)
    file(WRITE "${_panel_select_header}" "#pragma once\n#include \"${_panel_header}\"\n")
else()
    file(WRITE "${_panel_select_header}" "#pragma once\n#include \"panels/jd9165_jc1060_v2.h\"\n")
endif()

idf_component_register(
    SRCS
        "main.cpp"
        "../hal/hal_system_p4.cpp"
        "../hal/hal_display_p4.cpp"
        "../hal/hal_input_p4.cpp"
        "../hal/hal_audio_p4.cpp"
        "../hal/hal_network_p4.cpp"
        "../hal/hal_storage_p4.cpp"
        "../hal/hal_persistence_p4.cpp"
        "../hal/DisplayHAL.cpp"
        "../hal/TouchHAL.cpp"
        "../hal/AudioHAL.cpp"
        "../hal/LVGL_Port.cpp"
        "../hal/hal_flasher_p4.cpp"
        "../hal/usb_cdc_loader_port.cpp"
        "../hal/hal_usb_cdc_p4.cpp"
        "../hal/hal_usb_host_p4.cpp"
        "../hal/hal_usb_hid_host_p4.cpp"
        "../hal/hal_uart_p4.cpp"
        "../hal/hal_gpio_p4.cpp"
        "../hal/hal_mesh_p4.cpp"
        "../hal/hal_radio_p4.cpp"
        "../hal/hal_wifi_strike_p4.cpp"
        "../hal/hal_http_p4.cpp"
        "../hal/hal_socket_p4.cpp"
        "../hal/hal_ssh_p4.cpp"
        "../hal/hal_hid_p4.cpp"
        "../hal/hal_console_p4.cpp"
        "../hal/hal_lan_recon_p4.cpp"
        "../hal/hal_neighbor_discovery_p4.cpp"
        "../hal/EspIdfTimeProvider.cpp"
    INCLUDE_DIRS
        "."
        "../hal"
        "${_dt_gen_dir}"
        "${CMAKE_CURRENT_LIST_DIR}/../../.."
    REQUIRES
        "core"
        "skuodi__libssh2_esp"
        "esp_driver_i2c"
        "esp_driver_i2s"
        "esp_driver_gpio"
        "esp_driver_uart"
        "esp_driver_ledc"
        "esp_driver_ppa"
        "esp_driver_jpeg"
        "esp_driver_tsens"
        "esp_lcd"
        "espressif__esp_lcd_touch"
        "espressif__esp_lcd_touch_gt911"
        "espressif__esp_codec_dev"
        "espressif__esp_lvgl_port"
        "espressif__esp-serial-flasher"
        "espressif__usb_host_cdc_acm"
        "espressif__usb_host_cp210x_vcp"
        "espressif__usb_host_ch34x_vcp"
        "espressif__usb_host_hid"
        "espressif__esp_websocket_client"
        "espressif__esp_tinyusb"
        "usb"
        "lvgl__lvgl"
        "esp_driver_sdmmc"
        "sdmmc"
        "fatfs"
        "spiffs"
        "vfs"
        "esp_wifi"
        "esp_netif"
        "esp_event"
        "driver"
        "nvs_flash"
        "esp_timer"
        "esp_partition"
        "app_update"
        "spi_flash"
        "esp_mm"
        "esp-tls"
        "mbedtls"
        "freertos"
    WHOLE_ARCHIVE
)

add_custom_command(
    OUTPUT "${_dt_header}"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${_dt_gen_dir}"
    COMMAND python3 "${CMAKE_CURRENT_LIST_DIR}/../../../tools/cdtc.py" "${_dt_json}" "${_dt_header}"
    DEPENDS "${_dt_json}" "${CMAKE_CURRENT_LIST_DIR}/../../../tools/cdtc.py"
    COMMENT "Generando cbdos_device_tree.h para ${CBDOS_BOARD}"
)

add_custom_target(generate_board_config DEPENDS "${_dt_header}")
add_dependencies(${COMPONENT_LIB} generate_board_config)
