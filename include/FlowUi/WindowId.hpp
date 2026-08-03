#pragma once

#include <cstdint>

namespace FlowUi {

/** @brief Stable identity of a native FlowUi window. */
using WindowId = uint64_t;

/** @brief Invalid window identity and root attribution for app-shared storage. */
inline constexpr WindowId InvalidWindowId = 0;

/** @brief Identity of the initial semantic main application window. */
inline constexpr WindowId MainWindowId = 1;

} // namespace FlowUi
