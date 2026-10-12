#pragma once

#include "cbdos_build_profile.h"

#if CBDOS_FEATURE_SDR

namespace cbdos {
namespace sdr {

void registerSdrCliCommands();

} // namespace sdr
} // namespace cbdos

#endif // CBDOS_FEATURE_SDR
