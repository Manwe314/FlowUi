#pragma once
#include <cstdint>
#include <string_view>
namespace FlowUi {
/** Stable identity used by caller-provided capacity settings. */
using MemoryCapacityTargetId = uint64_t;
/** Compute the same FNV-1a target identity used by runtime and standalone
 * tooling. Hashes the exact key bytes without interning or allocation; returns
 * a nonzero value even for an empty name. Registration is checked separately.
 * @param name Registered, case-sensitive capacity target key.
 * @return Stable nonzero numeric identity. */
[[nodiscard]] constexpr MemoryCapacityTargetId
memory_capacity_target_id(std::string_view name) noexcept {
	uint64_t hash = 14695981039346656037ull;
	for (const char character : name) {
		hash ^= static_cast<unsigned char>(character);
		hash *= 1099511628211ull;
	}
	return hash == 0 ? 1 : hash;
}
/** Create a stable capacity target identity from its registered key. */
[[nodiscard]] consteval MemoryCapacityTargetId
makeMemoryCapacityTargetId(std::string_view name) noexcept {
	return memory_capacity_target_id(name);
}
} // namespace FlowUi
