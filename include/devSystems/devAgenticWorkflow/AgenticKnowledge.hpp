#pragma once
#include "FlowUi/BuildConfig.hpp"
#if FLOW_UI_AGENTIC_WORKFLOW
#include "devSystems/devAgenticWorkflow/AgenticTypes.hpp"

namespace FlowUi::devSystems::agentic {

/** Finite installed semantic pack lookup, usable without library source or a GPU. */
class AgenticKnowledge {
public:
	explicit AgenticKnowledge(std::filesystem::path pack_path,
							  std::filesystem::path source_root = {});
	[[nodiscard]] const std::filesystem::path& pack_path() const noexcept;
	[[nodiscard]] Result<Json> find(std::string_view intent, std::uint64_t maximum_bytes = 12000,
									std::uint32_t maximum_topics = 3) const noexcept;
	[[nodiscard]] Result<Json> read(std::string_view topic_id,
									std::uint64_t maximum_bytes = 12000) const noexcept;
	/** Validate source selectors and content digests against the matching checkout. */
	[[nodiscard]] Result<Json> validate() const noexcept;

private:
	std::filesystem::path pack_path_;
	std::filesystem::path source_root_;
};

} // namespace FlowUi::devSystems::agentic

#endif // FLOW_UI_AGENTIC_WORKFLOW
