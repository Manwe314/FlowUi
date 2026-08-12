#include "HeadlessVulkanFixture.hpp"
#include "TestHarness.hpp"

#include <atomic>
#include <barrier>
#include <cstdint>
#include <exception>
#include <iostream>
#include <string_view>
#include <thread>
#include <type_traits>

#include "internal/ManagerStorage/ElementStorageController.hpp"
#include "internal/StorageSystem/FlowStorageSystem.hpp"
#include "managers/ElementManager.hpp"
#include "managers/structs/ElementManagerStructs.hpp"

namespace {

struct Parameters {};

inline FlowUi::detail::storage::IStorageSystem* gReentrantStorage = nullptr;

struct alignas(64) AppResources {
	inline static std::atomic<int> constructions = 0;
	inline static std::atomic<int> destructions = 0;

	FlowUi::App* owner = nullptr;
	int marker = 41;

	explicit AppResources(FlowUi::App& app) noexcept : owner(&app) {
		constructions.fetch_add(1, std::memory_order_relaxed);
	}
	AppResources() = delete;
	AppResources(const AppResources&) = delete;
	AppResources& operator=(const AppResources&) = delete;
	~AppResources() noexcept { destructions.fetch_add(1, std::memory_order_relaxed); }

	static void reset() noexcept {
		constructions.store(0, std::memory_order_relaxed);
		destructions.store(0, std::memory_order_relaxed);
	}
};

struct ConstructorPreferenceResources {
	inline static int appConstructions = 0;
	inline static int defaultConstructions = 0;

	ConstructorPreferenceResources() noexcept { ++defaultConstructions; }
	explicit ConstructorPreferenceResources(FlowUi::App&) noexcept { ++appConstructions; }
	~ConstructorPreferenceResources() noexcept = default;
};

struct FlakyResources {
	inline static bool fail = true;
	inline static int attempts = 0;
	inline static int destructions = 0;

	explicit FlakyResources(FlowUi::App&) {
		++attempts;
		if (fail) throw std::runtime_error("intentional resource construction failure");
	}
	~FlakyResources() noexcept { ++destructions; }
};

struct ReentrantResources {
	inline static int constructions = 0;
	inline static int destructions = 0;

	FlowUi::detail::storage::StringId marker = 0;

	explicit ReentrantResources(FlowUi::App&) {
		++constructions;
		marker = gReentrantStorage->intern("flowui.tests.phase-h.reentrant-construction");
	}
	~ReentrantResources() noexcept {
		++destructions;
		if (gReentrantStorage) {
			(void)gReentrantStorage->intern("flowui.tests.phase-h.reentrant-destruction");
		}
	}
};

struct RecursiveResources {
	inline static int attempts = 0;
	explicit RecursiveResources(FlowUi::App& app);
	~RecursiveResources() noexcept = default;
};

template <typename Resource, FlowUi::FlowDefinitionID DefinitionId>
struct ResourceElementDefinition {
	using Parameters = ::Parameters;
	using Resources = Resource;
	using BuildContext = FlowUi::ElementBuildContext<ResourceElementDefinition>;
	using InteractionContext = FlowUi::ElementInteractionContext<ResourceElementDefinition>;
	static constexpr FlowUi::FlowDefinitionID definitionId = DefinitionId;
	static void buildElement(BuildContext&) {}
};

template <FlowUi::FlowDefinitionID DefinitionId>
struct ResourceFreeElementDefinition {
	using Parameters = ::Parameters;
	using BuildContext = FlowUi::ElementBuildContext<ResourceFreeElementDefinition>;
	using InteractionContext = FlowUi::ElementInteractionContext<ResourceFreeElementDefinition>;
	static constexpr FlowUi::FlowDefinitionID definitionId = DefinitionId;
	static void buildElement(BuildContext&) {}
};

using ResourceDefinition = ResourceElementDefinition<
	AppResources, FlowUi::DefinitionID("flowui/tests/phase-h/resources")>;
using SecondResourceDefinition = ResourceElementDefinition<
	AppResources, FlowUi::DefinitionID("flowui/tests/phase-h/resources-second")>;
using ConstructorPreferenceDefinition = ResourceElementDefinition<
	ConstructorPreferenceResources,
	FlowUi::DefinitionID("flowui/tests/phase-h/constructor-preference")>;
using FlakyDefinition = ResourceElementDefinition<
	FlakyResources, FlowUi::DefinitionID("flowui/tests/phase-h/flaky")>;
using ResourceFreeDefinition = ResourceFreeElementDefinition<
	FlowUi::DefinitionID("flowui/tests/phase-h/resource-free")>;
using ReentrantDefinition = ResourceElementDefinition<
	ReentrantResources, FlowUi::DefinitionID("flowui/tests/phase-h/reentrant")>;
using RecursiveDefinition = ResourceElementDefinition<
	RecursiveResources, FlowUi::DefinitionID("flowui/tests/phase-h/recursive")>;

inline FlowUi::detail::manager_storage::ElementStorageController* gRecursiveController = nullptr;

RecursiveResources::RecursiveResources(FlowUi::App& app) {
	++attempts;
	static_cast<void>(gRecursiveController->resolveOrCreateResources(
		FlowUi::detail::element::elementDescriptor<RecursiveDefinition>, app, false));
}

inline constexpr ResourceDefinition kResourceElement{};
inline constexpr SecondResourceDefinition kSecondResourceElement{};
inline constexpr ConstructorPreferenceDefinition kConstructorPreferenceElement{};
inline constexpr FlakyDefinition kFlakyElement{};
inline constexpr ResourceFreeDefinition kResourceFreeElement{};
inline constexpr auto kMixedElements = FlowUi::elementSet(
	kResourceElement,
	kResourceFreeElement,
	kSecondResourceElement);

template <typename Context>
concept HasConstResources = requires(const Context& context) {
	{ context.resources() } -> std::same_as<const AppResources&>;
};

template <typename Context>
concept HasAnyResources = requires(const Context& context) {
	context.resources();
};

template <typename Manager>
concept HasPrepareAllRegistered = requires(Manager& manager) {
	manager.prepareAllRegistered();
};

static_assert(HasConstResources<ResourceDefinition::BuildContext>);
static_assert(HasConstResources<ResourceDefinition::InteractionContext>);
static_assert(!HasAnyResources<ResourceFreeDefinition::BuildContext>);
static_assert(!HasAnyResources<ResourceFreeDefinition::InteractionContext>);
static_assert(!HasPrepareAllRegistered<FlowUi::ElementManager>);
static_assert(std::same_as<
	decltype(kMixedElements),
	const FlowUi::ElementSet<
		ResourceDefinition,
		ResourceFreeDefinition,
		SecondResourceDefinition>>);
static_assert(std::is_empty_v<std::remove_cv_t<decltype(kMixedElements)>>);

class ResourceStore {
public:
	explicit ResourceStore(FlowUi::test::HeadlessVulkanFixture& vulkan)
		: storage_(vulkan.context()), controller_(storage_) {
		FlowUi::detail::storage::StorageConfig config{};
		config.initialPersistentCpuBytes = 4096;
		config.expectedPersistentRecords = 16;
		config.expectedWindows = 1;
		storage_.initialize(config);

		FlowUi::detail::storage::WindowStorageDesc window{};
		window.framesInFlight = 1;
		window.workerCount = 1;
		window.initialTextureBindings = 1;
		window.maxTextureBindings = 4;
		storage_.registerWindow(91, window);
	}

	~ResourceStore() {
		controller_.shutdown();
		storage_.unregisterWindow(91, 0);
		storage_.shutdown();
	}

	template <FlowUi::FlowElement Element>
	[[nodiscard]] const FlowUi::ResourcesOf<Element>& resolve(
		FlowUi::App& app,
		bool retryFailed = true) {
		return *static_cast<const FlowUi::ResourcesOf<Element>*>(
			controller_.resolveOrCreateResources(
				FlowUi::detail::element::elementDescriptor<Element>, app, retryFailed));
	}

	[[nodiscard]] FlowUi::detail::storage::FrameToken beginStorageFrame() {
		return storage_.beginFrame(91, {.frameSlot = 0, .frameNumber = 1});
	}

	[[nodiscard]] FlowUi::detail::storage::FrameReadLease sealStorageFrame(
		const FlowUi::detail::storage::FrameToken& frame) {
		return storage_.sealFrame(frame);
	}

	void cancelStorageFrame(const FlowUi::detail::storage::FrameToken& frame) noexcept {
		storage_.cancelFrame(frame);
	}

	[[nodiscard]] FlowUi::detail::storage::IStorageSystem& storage() noexcept {
		return storage_;
	}

	[[nodiscard]] FlowUi::detail::manager_storage::ElementStorageController&
	controller() noexcept {
		return controller_;
	}

private:
	FlowUi::detail::storage::FlowStorageSystem storage_;
	FlowUi::detail::manager_storage::ElementStorageController controller_;
};

void compilePublicPrepareSurface() {
	FlowUi::ElementManager uninitialized;
	// Resource-free preparation is a complete no-op, including on an uninitialized manager.
	uninitialized.prepare(kResourceFreeElement);
}

void compileResourcefulPrepareCalls(FlowUi::ElementManager& manager) {
	manager.prepare(kResourceElement);
	manager.prepare(kMixedElements);
}

void testResourcesConstructOncePerDefinitionAcrossThreads(
	FlowUi::test::HeadlessVulkanFixture& vulkan) {
	AppResources::reset();
	FlowUi::App app;
	{
		ResourceStore store(vulkan);
		std::barrier start(3);
		const AppResources* first = nullptr;
		const AppResources* second = nullptr;
		std::thread firstThread([&] {
			start.arrive_and_wait();
			first = &store.resolve<ResourceDefinition>(app);
		});
		std::thread secondThread([&] {
			start.arrive_and_wait();
			second = &store.resolve<ResourceDefinition>(app);
		});
		start.arrive_and_wait();
		firstThread.join();
		secondThread.join();

		FLOWUI_CHECK(first != nullptr);
		FLOWUI_CHECK(first == second);
		FLOWUI_CHECK(first->owner == &app);
		FLOWUI_CHECK(reinterpret_cast<uintptr_t>(first) % alignof(AppResources) == 0);
		FLOWUI_CHECK(AppResources::constructions.load(std::memory_order_relaxed) == 1);

		const AppResources& otherDefinition =
			store.resolve<SecondResourceDefinition>(app);
		FLOWUI_CHECK(&otherDefinition != first);
		FLOWUI_CHECK(AppResources::constructions.load(std::memory_order_relaxed) == 2);
	}
	FLOWUI_CHECK(AppResources::destructions.load(std::memory_order_relaxed) == 2);
}

void testAppConstructorIsPreferred(FlowUi::test::HeadlessVulkanFixture& vulkan) {
	ConstructorPreferenceResources::appConstructions = 0;
	ConstructorPreferenceResources::defaultConstructions = 0;
	FlowUi::App app;
	ResourceStore store(vulkan);
	static_cast<void>(store.resolve<ConstructorPreferenceDefinition>(app));
	FLOWUI_CHECK(ConstructorPreferenceResources::appConstructions == 1);
	FLOWUI_CHECK(ConstructorPreferenceResources::defaultConstructions == 0);
}

void testFailedConstructionCanBeRetriedByPreparation(
	FlowUi::test::HeadlessVulkanFixture& vulkan) {
	FlakyResources::fail = true;
	FlakyResources::attempts = 0;
	FlakyResources::destructions = 0;
	FlowUi::App app;
	ResourceStore store(vulkan);

	try {
		static_cast<void>(store.resolve<FlakyDefinition>(app, false));
	} catch (const std::runtime_error& error) {
		const std::string_view message = error.what();
		FLOWUI_CHECK(message.find("prepare(element)") != std::string_view::npos);
		FlakyResources::fail = false;
		static_cast<void>(store.resolve<FlakyDefinition>(app, true));
		FLOWUI_CHECK(FlakyResources::attempts == 2);
		return;
	}
	throw FlowUi::test::CheckFailure("resource construction failure was not reported");
}

void testActiveStorageFrameProducesPreparationDiagnostic(
	FlowUi::test::HeadlessVulkanFixture& vulkan) {
	AppResources::reset();
	FlowUi::App app;
	ResourceStore store(vulkan);
	const auto frame = store.beginStorageFrame();
	[[maybe_unused]] const auto lease = store.sealStorageFrame(frame);
	try {
		static_cast<void>(store.resolve<ResourceDefinition>(app, false));
	} catch (const std::runtime_error& error) {
		FLOWUI_CHECK(std::string_view(error.what()).find("before beginFrame") !=
			std::string_view::npos);
		store.cancelStorageFrame(frame);
		static_cast<void>(store.resolve<ResourceDefinition>(app, true));
		FLOWUI_CHECK(AppResources::constructions.load(std::memory_order_relaxed) == 1);
		return;
	}
	store.cancelStorageFrame(frame);
	throw FlowUi::test::CheckFailure("active-frame resource mutation was accepted");
}

void testResourceLifecycleCanReenterStorage(
	FlowUi::test::HeadlessVulkanFixture& vulkan) {
	ReentrantResources::constructions = 0;
	ReentrantResources::destructions = 0;
	FlowUi::App app;
	{
		ResourceStore store(vulkan);
		gReentrantStorage = &store.storage();
		const ReentrantResources& resources = store.resolve<ReentrantDefinition>(app);
		FLOWUI_CHECK(resources.marker != 0);
		FLOWUI_CHECK(ReentrantResources::constructions == 1);
	}
	gReentrantStorage = nullptr;
	FLOWUI_CHECK(ReentrantResources::destructions == 1);
}

bool exceptionChainContains(const std::exception& error, std::string_view text) {
	if (std::string_view(error.what()).find(text) != std::string_view::npos) return true;
	try {
		std::rethrow_if_nested(error);
	} catch (const std::exception& nested) {
		return exceptionChainContains(nested, text);
	} catch (...) {
	}
	return false;
}

void testRecursiveConstructionIsRejected(
	FlowUi::test::HeadlessVulkanFixture& vulkan) {
	RecursiveResources::attempts = 0;
	FlowUi::App app;
	ResourceStore store(vulkan);
	gRecursiveController = &store.controller();
	try {
		static_cast<void>(store.resolve<RecursiveDefinition>(app, false));
	} catch (const std::exception& error) {
		gRecursiveController = nullptr;
		FLOWUI_CHECK(exceptionChainContains(error, "recursively requested"));
		FLOWUI_CHECK(RecursiveResources::attempts == 1);
		return;
	}
	gRecursiveController = nullptr;
	throw FlowUi::test::CheckFailure("recursive resource construction was accepted");
}

} // namespace

int main() {
	FlowUi::test::Runner runner;
	try {
		FlowUi::test::HeadlessVulkanFixture vulkan;
		runner.run("public prepare surface and resource-free no-op compile", [] {
			compilePublicPrepareSurface();
			[[maybe_unused]] auto* compileOnly = &compileResourcefulPrepareCalls;
		});
		runner.run("resources construct once per definition across threads", [&] {
			testResourcesConstructOncePerDefinitionAcrossThreads(vulkan);
		});
		runner.run("Resources(App&) is preferred over default construction", [&] {
			testAppConstructorIsPreferred(vulkan);
		});
		runner.run("failed lazy construction can be retried by preparation", [&] {
			testFailedConstructionCanBeRetriedByPreparation(vulkan);
		});
		runner.run("active-frame construction reports eager preparation guidance", [&] {
			testActiveStorageFrameProducesPreparationDiagnostic(vulkan);
		});
		runner.run("resource construction and destruction can reenter storage", [&] {
			testResourceLifecycleCanReenterStorage(vulkan);
		});
		runner.run("recursive resource construction is rejected", [&] {
			testRecursiveConstructionIsRejected(vulkan);
		});
		return runner.finish();
	} catch (const FlowUi::test::VulkanUnavailable& error) {
#ifdef FLOWUI_TEST_REQUIRE_VULKAN_DEVICE
		std::cerr << "FAIL: " << error.what() << '\n';
		return 1;
#else
		std::cout << "SKIP: " << error.what() << '\n';
		return 77;
#endif
	}
}
