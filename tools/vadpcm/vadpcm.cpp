// Copyright 2022 Dietrich Epp.
// This file is part of Skelly 64. Skelly 64 is licensed under the terms of the
// Mozilla Public License, version 2.0. See LICENSE.txt for details.
#include "tools/vadpcm/vadpcm.hpp"

#include <fmt/format.h>

namespace vadpcm {

std::string VADPCMErrorMessage(vadpcm_error err) {
    const char *msg = vadpcm_error_name(err);
    if (msg != nullptr) {
        return msg;
    }
    return fmt::format("unknown error: {}", static_cast<int>(err));
}

} // namespace vadpcm
