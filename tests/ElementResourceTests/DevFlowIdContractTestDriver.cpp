#include "ElementResourceContractTestDriver.hpp"

#include <memory>
#include <stdexcept>
#include <string>
#include <utility>

#include "FlowUi/BuildConfig.hpp"
#include "managers/UiManager.hpp"
#include "internal/ManagerStorage/UiManagerState.hpp"

#if !FLOW_UI_DEV_MODE
#error "DevFlowIdContractTestDriver must only be compiled with FLOW_UI_DEV_MODE=1."
#endif

namespace FlowUi::test::element_resource {

namespace {

class DevFlowIdContractDriverImpl final : public DevFlowIdContractDriver {
public:
	void beginFrame() override {
		tracker_.beginFrame();
	}

	ClaimDisposition claim(
		FlowElementID elementId,
		FlowDefinitionID definitionId,
		std::string_view debugPath) override {
		(void)tracker_.claim(
			detail::element::toInstanceKey(elementId),
			detail::manager_storage::FlowRootClaimSourceForDev{
				.definitionId = definitionId,
				.debugPath = std::string(debugPath),
				.fileName = "PhaseADevFlowIdTests.cpp",
				.functionName = "DevFlowIdContractDriverImpl::claim",
				.line = 1,
				.column = 1,
			});
		return ClaimDisposition::Continue;
	}

	[[nodiscard]] std::size_t warningCount() const noexcept override {
		return tracker_.collisionCount();
	}

	[[nodiscard]] const FlowIdCollisionWarning& warning(std::size_t index) const override {
		const detail::manager_storage::FlowRootCollisionForDev& collision =
			tracker_.collision(index);
		warning_ = FlowIdCollisionWarning{
			.elementId = FlowElementID{
				.value = collision.instanceId.value,
				.debugName = collision.duplicate.debugPath,
			},
			.firstDefinition = collision.first.definitionId,
			.duplicateDefinition = collision.duplicate.definitionId,
			.debugPath = collision.duplicate.debugPath,
			.firstDebugPath = collision.first.debugPath,
			.firstFileName = collision.first.fileName,
			.duplicateFileName = collision.duplicate.fileName,
			.firstLine = collision.first.line,
			.duplicateLine = collision.duplicate.line,
		};
		return warning_;
	}

private:
	detail::manager_storage::FlowRootIdTrackerForDev tracker_{};
	mutable FlowIdCollisionWarning warning_{};
};

} // namespace

std::unique_ptr<DevFlowIdContractDriver> makeDevFlowIdContractDriver() {
	return std::make_unique<DevFlowIdContractDriverImpl>();
}

} // namespace FlowUi::test::element_resource
