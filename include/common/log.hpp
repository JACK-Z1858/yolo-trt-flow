// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include <string_view>

namespace common {
enum class LogLevel { Info, Warning, Error };
// Timestamp, level, module, and thread ID; serializes concurrent writes.
void log(LogLevel level, std::string_view module, std::string_view message) noexcept;
}  // namespace common
