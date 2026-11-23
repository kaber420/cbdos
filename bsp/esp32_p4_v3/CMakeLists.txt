cmake_minimum_required(VERSION 3.16)

set(CBDOS_BOARD "jc1060p470" CACHE STRING "Placa CBDos ESP32-P4 v3+")
set(ENV{CBDOS_BOARD} "${CBDOS_BOARD}")

set(EXTRA_COMPONENT_DIRS
    "${CMAKE_CURRENT_LIST_DIR}/../../core"
)

set(_base_defaults "${CMAKE_CURRENT_LIST_DIR}/sdkconfig.defaults")
set(_board_defaults "${CMAKE_CURRENT_LIST_DIR}/sdkconfig.defaults.${CBDOS_BOARD}")
set(_composed_defaults "${_base_defaults}")
if(EXISTS "${_board_defaults}")
    set(_composed_defaults "${_composed_defaults};${_board_defaults}")
endif()

# Aislamiento de perfiles por carpeta de compilacion (-B build/<perfil> o -B build_<perfil>)
set(SDKCONFIG "${CMAKE_BINARY_DIR}/sdkconfig")
if(CMAKE_BINARY_DIR MATCHES "(/red|build_red)$")
    message(STATUS "[CBDos-P4-v3] Board=${CBDOS_BOARD} | Perfil: CYBERDECK RED")
    set(SDKCONFIG_DEFAULTS "${_composed_defaults};${CMAKE_CURRENT_LIST_DIR}/sdkconfig.red" CACHE STRING "" FORCE)
elseif(CMAKE_BINARY_DIR MATCHES "(/lite|build_lite)$")
    message(STATUS "[CBDos-P4-v3] Board=${CBDOS_BOARD} | Perfil: CYBERDECK LITE")
    set(SDKCONFIG_DEFAULTS "${_composed_defaults};${CMAKE_CURRENT_LIST_DIR}/sdkconfig.lite" CACHE STRING "" FORCE)
elseif(CMAKE_BINARY_DIR MATCHES "(/kds|build_kds)$")
    message(STATUS "[CBDos-P4-v3] Board=${CBDOS_BOARD} | Perfil: TABLEHUB KDS")
    set(SDKCONFIG_DEFAULTS "${_composed_defaults};${CMAKE_CURRENT_LIST_DIR}/sdkconfig.kds" CACHE STRING "" FORCE)
elseif(CMAKE_BINARY_DIR MATCHES "(/tabletop|build_tabletop)$")
    message(STATUS "[CBDos-P4-v3] Board=${CBDOS_BOARD} | Perfil: TABLEHUB TABLETOP")
    set(SDKCONFIG_DEFAULTS "${_composed_defaults};${CMAKE_CURRENT_LIST_DIR}/sdkconfig.tabletop" CACHE STRING "" FORCE)
else()
    message(STATUS "[CBDos-P4-v3] Board=${CBDOS_BOARD} | Perfil: CYBERDECK FULL")
    set(SDKCONFIG_DEFAULTS "${_composed_defaults}" CACHE STRING "" FORCE)
endif()

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(cbdos_p4_v3)
target_link_options(${CMAKE_PROJECT_NAME}.elf PRIVATE "-Wl,--allow-multiple-definition")
