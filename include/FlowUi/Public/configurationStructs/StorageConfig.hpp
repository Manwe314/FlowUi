#pragma once

#include <cstdint>

namespace FlowUi {

/**
 * Typed startup capacities for storage pools and buffers, measured in bytes. Zero preserves defaults.
 * MemoryCapacityProfile::settings overrides matching hints; enabled baked capacities override both.
 * Later storage growth follows the profile and error policy; these values set initial reserves.
 */
struct StorageMemoryCapacities {
	/** Initial persistent CPU pool size in bytes; zero uses the library default. */
	uint64_t initialPersistentCpuBytes = 0u;
	/** Initial interned-string storage size in bytes; zero uses the library default. */
	uint64_t initialStringBytes = 0u;
	/** Initial transient bytes reserved for each frame slot of each window; zero uses the default. */
	uint64_t transientBytesPerFramePerWindow = 0u;
	/** Initial scratch bytes per storage worker; zero uses the library default. */
	uint64_t transientBytesPerWorker = 0u;
	/** Initial image/font decode scratch capacity in bytes; zero uses the default. */
	uint64_t initialDecodeScratchBytes = 0u;
	/** Initial GPU upload staging capacity in bytes; zero uses the default. */
	uint64_t initialUploadStagingBytes = 0u;
	/** Initial renderer instance-buffer bytes per frame slot; zero uses the default. */
	uint64_t initialInstanceBytesPerFrame = 0u;
};

} // namespace FlowUi
