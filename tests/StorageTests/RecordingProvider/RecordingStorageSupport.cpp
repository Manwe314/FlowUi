#include <atomic>
#include <cstdint>

namespace FlowUi::test {
namespace {
std::atomic_uint64_t constructionCount{0};
}

/** Record construction in the separately linked provider dependency. */
void noteRecordingStorageConstruction() noexcept {
	++constructionCount;
}

/** Observe the provider dependency's actual lifecycle work. */
[[nodiscard]] uint64_t recordingStorageConstructionCount() noexcept {
	return constructionCount.load();
}
} // namespace FlowUi::test
