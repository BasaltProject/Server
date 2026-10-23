#pragma once

#include <string>

namespace basalt::server::runtime {

/// What a join or quit announcement turns into once plugins have had their say: empty text keeps the vanilla line.
struct Announcement {
    std::string text;
    bool silent = false;
};

} // namespace basalt::server::runtime
