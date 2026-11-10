#pragma once

#include <cstdint>

namespace FlowUi {

/**
 * Typed initial storage capacities in bytes. Zero hints preserve defaults; these are startup reserves
 * rather than hard memory limits.
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
