#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string_view>

#include "FlowUi/App.hpp"
#include "FlowUi/WindowId.hpp"

namespace FlowUi::test::element_resource {

// Phase A intentionally freezes behavior before ElementStorageController exists.
// The implementation phase must provide thin adapters for these contracts; the
// contracts are not proposed public FlowUi APIs.

struct PrimaryState {
	static inline int constructions = 0;
	static inline int destructions = 0;

	int value = 0;

	PrimaryState() { ++constructions; }
	PrimaryState(const PrimaryState&) = delete;
	PrimaryState& operator=(const PrimaryState&) = delete;
	PrimaryState(PrimaryState&&) = delete;
	PrimaryState& operator=(PrimaryState&&) = delete;
	~PrimaryState() noexcept { ++destructions; }

	static void resetCounts() noexcept {
		constructions = 0;
		destructions = 0;
	}
};

struct AlternateState {
	static inline int constructions = 0;
	static inline int destructions = 0;

	float value = 0.0f;

	AlternateState() { ++constructions; }
	AlternateState(const AlternateState&) = delete;
	AlternateState& operator=(const AlternateState&) = delete;
	AlternateState(AlternateState&&) = delete;
	AlternateState& operator=(AlternateState&&) = delete;
	~AlternateState() noexcept { ++destructions; }

	static void resetCounts() noexcept {
		constructions = 0;
		destructions = 0;
	}
};

struct SharedResources {
	static inline int constructions = 0;
	static inline int destructions = 0;

	explicit SharedResources(FlowUi::App&) { ++constructions; }
	SharedResources(const SharedResources&) = delete;
	SharedResources& operator=(const SharedResources&) = delete;
	SharedResources(SharedResources&&) = delete;
	SharedResources& operator=(SharedResources&&) = delete;
	~SharedResources() noexcept { ++destructions; }

	static void resetCounts() noexcept {
		constructions = 0;
		destructions = 0;
	}
};

struct PrimaryElement {
	using State = PrimaryState;
	static constexpr FlowDefinitionID definitionId =
		DefinitionID("flowui/tests/element-resource/primary");
	static constexpr std::string_view debugName = "PrimaryElement";
};

struct AlternateElement {
	using State = AlternateState;
	static constexpr FlowDefinitionID definitionId =
		DefinitionID("flowui/tests/element-resource/alternate");
	static constexpr std::string_view debugName = "AlternateElement";
};

struct ResourceElement {
	using Resources = SharedResources;
	static constexpr FlowDefinitionID definitionId =
		DefinitionID("flowui/tests/element-resource/resources");
	static constexpr std::string_view debugName = "ResourceElement";
};

struct StatelessElement {
	static constexpr FlowDefinitionID definitionId =
		DefinitionID("flowui/tests/element-resource/stateless");
	static constexpr std::string_view debugName = "StatelessElement";
};

enum class TestStateRetention : uint8_t {
	Transient,
	WindowLifetime,
};

struct TestStatePolicy {
	TestStateRetention retention = TestStateRetention::Transient;
	uint32_t graceFrames = 0;

	static constexpr TestStatePolicy transient(uint32_t grace = 0) noexcept {
		return {.retention = TestStateRetention::Transient, .graceFrames = grace};
	}

	static constexpr TestStatePolicy windowLifetime() noexcept {
		return {.retention = TestStateRetention::WindowLifetime, .graceFrames = 0};
	}
};

struct StateRecordMetadataView {
	FlowElementID flowId{};
	FlowDefinitionID definitionId{};
	uint64_t stateTypeHash = 0;
};

class ElementControllerContractDriver {
public:
	virtual ~ElementControllerContractDriver() = default;

	virtual void registerWindow(WindowId window) = 0;
	virtual void destroyWindow(WindowId window) = 0;

	virtual void beginFrame(WindowId window, uint64_t frameNumber) = 0;
	virtual void commitFrame(WindowId window) noexcept = 0;
	virtual void cancelFrame(WindowId window) noexcept = 0;

	virtual PrimaryState& resolvePrimary(
		WindowId window,
		FlowElementID flowId,
		TestStatePolicy policy = TestStatePolicy::transient()) = 0;
	virtual AlternateState& resolveAlternate(
		WindowId window,
		FlowElementID flowId,
		TestStatePolicy policy = TestStatePolicy::transient()) = 0;

	[[nodiscard]] virtual PrimaryState* findPrimary(
		WindowId window,
		FlowElementID flowId) = 0;
	[[nodiscard]] virtual AlternateState* findAlternate(
		WindowId window,
		FlowElementID flowId) = 0;
	[[nodiscard]] virtual StateRecordMetadataView metadata(
		WindowId window,
		FlowElementID flowId) const = 0;

	virtual bool erasePrimary(WindowId window, FlowElementID flowId) = 0;
	virtual void collectAllEligible(WindowId window) noexcept = 0;
	[[nodiscard]] virtual std::size_t liveStateCount(WindowId window) const = 0;

	// requestingWindow models two UiManagers reaching the same app-wide
	// definition resource. It is intentionally not part of resource identity.
	virtual const SharedResources& resolveSharedResources(WindowId requestingWindow) = 0;
};

// Supplied with the real ElementStorageController implementation. Keeping the
// adapter in tests allows the controller's private API to evolve while these
// semantic contracts remain unchanged.
std::unique_ptr<ElementControllerContractDriver> makeElementControllerContractDriver();

enum class ClaimDisposition : uint8_t {
	Continue,
};

struct FlowIdCollisionWarning {
	FlowElementID elementId{};
	FlowDefinitionID firstDefinition{};
	FlowDefinitionID duplicateDefinition{};
	std::string_view debugPath{};
	std::string_view firstDebugPath{};
	std::string_view firstFileName{};
	std::string_view duplicateFileName{};
	uint32_t firstLine = 0;
	uint32_t duplicateLine = 0;
};

class DevFlowIdContractDriver {
public:
	virtual ~DevFlowIdContractDriver() = default;

	virtual void beginFrame() = 0;
	virtual ClaimDisposition claim(
		FlowElementID elementId,
		FlowDefinitionID definitionId,
		std::string_view debugPath) = 0;

	[[nodiscard]] virtual std::size_t warningCount() const noexcept = 0;
	[[nodiscard]] virtual const FlowIdCollisionWarning& warning(std::size_t index) const = 0;
};

// One driver represents the dev-only tracker owned by one UiManager.
std::unique_ptr<DevFlowIdContractDriver> makeDevFlowIdContractDriver();

} // namespace FlowUi::test::element_resource
