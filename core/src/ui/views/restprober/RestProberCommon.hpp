#pragma once

#include "cbdos/http.hpp"
#include <cstdint>

namespace cbdos {
namespace ui {
namespace restprober {

// Paleta cromática Cyberdeck para RestProber
static constexpr uint32_t kColBgCard    = 0x1E2230;
static constexpr uint32_t kColBorder    = 0x2E344A;
static constexpr uint32_t kColGreen     = 0x10B981;
static constexpr uint32_t kColBlue      = 0x3B82F6;
static constexpr uint32_t kColYellow    = 0xF59E0B;
static constexpr uint32_t kColRed       = 0xEF4444;
static constexpr uint32_t kColPurple    = 0x8B5CF6;
static constexpr uint32_t kColGray      = 0x64748B;
static constexpr uint32_t kColTextMuted = 0x94A3B8;

inline uint32_t getMethodColor(http::HttpMethod method) {
    switch (method) {
        case http::HttpMethod::Get:    return kColGreen;
        case http::HttpMethod::Post:   return kColBlue;
        case http::HttpMethod::Put:    return kColYellow;
        case http::HttpMethod::Delete: return kColRed;
        case http::HttpMethod::Patch:  return kColPurple;
        case http::HttpMethod::Head:   return kColGray;
        default:                       return kColGreen;
    }
}

} // namespace restprober
} // namespace ui
} // namespace cbdos
