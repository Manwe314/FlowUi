#include "devSystems/devMonitoringAndReporting/reporting/DevErrorAdvice.hpp"

#if FLOW_UI_DEV_MODE

#include <algorithm>
#include <array>

namespace FlowUi::devSystems {
namespace {

// This is the single source of human-facing DEV error guidance. Keep the prose
// concise, actionable, and honest about evidence limitations. The engine must not
// synthesize replacement wording elsewhere.

constexpr std::string_view kCapacityExplanation =
	"The operation required more bounded capacity than the current configuration allowed.";
constexpr std::string_view kCapacityAction =
	"Inspect the captured demand and memory history, then increase the relevant capacity or reduce retained work.";
constexpr std::string_view kCapacityLimitation =
	"Capacity values may be incomplete when no state snapshot or memory checkpoint was retained.";

constexpr std::string_view kFrameExplanation =
	"The frame lifecycle was used from a phase that does not permit this operation.";
constexpr std::string_view kFrameAction =
	"Follow beginFrame, UI construction, endFrame, and drawFrame in order, and do not overlap active window frames.";
constexpr std::string_view kFrameLimitation =
	"The report identifies the observed frame context; application-side calls outside FlowUi require DevTooling scopes for a fuller path.";

constexpr std::string_view kResourceExplanation =
	"A resource lookup or publication did not match the resource identity and lifetime expected by this operation.";
constexpr std::string_view kResourceAction =
	"Verify the resource key, owning window, kind, and generation, then recreate or retain the resource for the required lifetime.";
constexpr std::string_view kResourceLimitation =
	"A precise lifetime cause requires a matching retained memory/resource event or snapshot.";

constexpr std::string_view kAssetExplanation =
	"The asset could not be opened, decoded, or validated in the format required by FlowUi.";
constexpr std::string_view kAssetAction =
	"Verify the resolved path, file accessibility, supported format, and generated asset payload before loading it again.";
constexpr std::string_view kAssetLimitation =
	"FlowUi cannot determine how an external asset was produced when no native decoder diagnostic was supplied.";

constexpr std::string_view kRendererExplanation =
	"Renderer input or retained native state did not satisfy the renderer contract for this frame.";
constexpr std::string_view kRendererAction =
	"Inspect the frame timeline and render-command evidence, then correct the invalid command, stale generation, or configured capacity.";
constexpr std::string_view kRendererLimitation =
	"GPU-side state is limited to evidence captured before submission and any retained Vulkan diagnostic.";

constexpr std::string_view kVulkanCapabilityExplanation =
	"The selected Vulkan environment does not expose a capability required by the requested configuration.";
constexpr std::string_view kVulkanCapabilityAction =
	"Compare the requested extensions, features, and Vulkan version with the selected device; disable only optional features or select a compatible device.";
constexpr std::string_view kVulkanNativeExplanation =
	"A Vulkan operation returned a native failure code while executing the identified FlowUi operation.";
constexpr std::string_view kVulkanNativeAction =
	"Inspect the native result, validation output, raw stack, and the surrounding frame or submission before changing synchronization or resource lifetime.";
constexpr std::string_view kVulkanLimitation =
	"The native result narrows the failing API operation but does not by itself prove the originating synchronization or lifetime defect.";

constexpr std::string_view kPopupExplanation =
	"Popup submission conflicted with the current popup frame, identity, anchor, or configured capacity.";
constexpr std::string_view kPopupAction =
	"Submit each popup identity once during an active frame, keep its anchor alive, and apply a deliberate duplicate/capacity policy.";
constexpr std::string_view kPopupLimitation =
	"Both application submission sources are available only when future DevTooling scopes are enabled.";

constexpr std::string_view kViewportExplanation =
	"The requested viewport was missing, detached, incomplete, or failed while recording its render target.";
constexpr std::string_view kViewportAction =
	"Verify viewport ownership and generation, keep it attached through the frame, and inspect whether fallback or skip policy was applied.";
constexpr std::string_view kViewportLimitation =
	"External rendering work is visible only through retained submission, timing, and native diagnostic evidence.";

constexpr std::string_view kMemoryPressureTitle = "Captured memory growth preceded the capacity failure";
constexpr std::string_view kMemoryPressureExplanation =
	"A retained growth or allocation event shares the error's resource, submission, or frame context.";
constexpr std::string_view kMemoryPressureAction =
	"Inspect the pinned memory event and checkpoint before changing the configured budget or retention policy.";
constexpr std::string_view kMemoryPressureLimitation =
	"Shared context is strong diagnostic evidence, not proof that the memory event caused the error.";

constexpr std::string_view kFallbackTitle = "FlowUi applied a fallback or skip policy";
constexpr std::string_view kFallbackExplanation =
	"The operation completed through a configured fallback, skip, recreation, or retry resolution.";
constexpr std::string_view kFallbackAction =
	"Review whether this production policy is acceptable; use the captured occurrence to diagnose why the preferred path was unavailable.";
constexpr std::string_view kFallbackLimitation =
	"A successful fallback limits impact but does not remove the underlying failure condition.";

constexpr std::string_view kCaptureTitle = "Capture more evidence before selecting a cause";
constexpr std::string_view kCaptureExplanation =
	"Relevant stack, snapshot, timing, memory, or breadcrumb history was unavailable, truncated, or evicted.";
constexpr std::string_view kCaptureAction =
	"Reproduce with StackAndState capture and a larger bounded retention window for the affected code/site group.";
constexpr std::string_view kCaptureLimitation =
	"The current report can describe the contract failure but cannot distinguish the remaining likely causes.";

constexpr auto kCatalogue = std::to_array<DevErrorAdviceDescriptor>({
	// Evidence-sensitive rules rank above code-wide guidance.
	{0x1001u, ErrorCode::StorageCapacityExceeded, ErrorSite::ResourceAllocatePersistent,
		DevErrorAdviceCategory::Capacity, DevErrorAdvicePredicate::MemoryPressureObserved,
		950u, DevErrorAdviceConfidence::Strong, kMemoryPressureTitle,
		kMemoryPressureExplanation, kMemoryPressureAction, "errors.policy.persistentCapacity",
		"flowui://docs/storage/capacity", kMemoryPressureLimitation},
	{0x1002u, ErrorCode::StorageBudgetExceeded, ErrorSite::None,
		DevErrorAdviceCategory::Capacity, DevErrorAdvicePredicate::MemoryPressureObserved,
		940u, DevErrorAdviceConfidence::Strong, kMemoryPressureTitle,
		kMemoryPressureExplanation, kMemoryPressureAction, "storage.memoryBudget",
		"flowui://docs/storage/budgets", kMemoryPressureLimitation},
	{0x1003u, ErrorCode::ViewportNotFound, ErrorSite::None,
		DevErrorAdviceCategory::Viewport, DevErrorAdvicePredicate::FallbackApplied,
		900u, DevErrorAdviceConfidence::Certain, kFallbackTitle, kFallbackExplanation,
		kFallbackAction, "errors.policy.missingViewport",
		"flowui://docs/viewports/missing-policy", kFallbackLimitation},
	{0x1004u, ErrorCode::AssetDecodeFailed, ErrorSite::None,
		DevErrorAdviceCategory::Asset, DevErrorAdvicePredicate::NativeCodeAvailable,
		880u, DevErrorAdviceConfidence::Strong, "The decoder supplied a native failure code",
		"The asset decoder identified a native failure while FlowUi was validating the payload.",
		"Use the native code and copied diagnostic text to verify encoding, channel layout, dimensions, and decoder support.",
		{}, "flowui://docs/assets/decoding", kAssetLimitation},

	// Capacity contracts.
	{0x1101u, ErrorCode::FrameCapacityExceeded, ErrorSite::None, DevErrorAdviceCategory::Capacity,
		DevErrorAdvicePredicate::Always, 700u, DevErrorAdviceConfidence::Possible,
		"Frame capacity was exhausted", kCapacityExplanation, kCapacityAction,
		"storage.frameTransientCapacity", "flowui://docs/frames/capacity", kCapacityLimitation},
	{0x1102u, ErrorCode::RendererCapacityExceeded, ErrorSite::None, DevErrorAdviceCategory::Capacity,
		DevErrorAdvicePredicate::Always, 700u, DevErrorAdviceConfidence::Possible,
		"Renderer capacity was exhausted", kCapacityExplanation, kCapacityAction,
		"renderer.capacity", "flowui://docs/renderer/capacity", kCapacityLimitation},
	{0x1103u, ErrorCode::StorageBudgetExceeded, ErrorSite::None, DevErrorAdviceCategory::Capacity,
		DevErrorAdvicePredicate::Always, 690u, DevErrorAdviceConfidence::Possible,
		"Storage memory budget was exceeded", kCapacityExplanation, kCapacityAction,
		"storage.memoryBudget", "flowui://docs/storage/budgets", kCapacityLimitation},
	{0x1104u, ErrorCode::StorageCapacityExceeded, ErrorSite::None, DevErrorAdviceCategory::Capacity,
		DevErrorAdvicePredicate::Always, 690u, DevErrorAdviceConfidence::Possible,
		"Storage capacity was exhausted", kCapacityExplanation, kCapacityAction,
		"errors.policy.capacity", "flowui://docs/storage/capacity", kCapacityLimitation},
	{0x1105u, ErrorCode::FontAtlasCapacityExceeded, ErrorSite::None, DevErrorAdviceCategory::Capacity,
		DevErrorAdvicePredicate::Always, 680u, DevErrorAdviceConfidence::Possible,
		"Font atlas capacity was exhausted", kCapacityExplanation, kCapacityAction,
		"font.atlasCapacity", "flowui://docs/fonts/atlas-capacity", kCapacityLimitation},
	{0x1106u, ErrorCode::IconAtlasCapacityExceeded, ErrorSite::None, DevErrorAdviceCategory::Capacity,
		DevErrorAdvicePredicate::Always, 680u, DevErrorAdviceConfidence::Possible,
		"Icon atlas capacity was exhausted", kCapacityExplanation, kCapacityAction,
		"icon.atlasCapacity", "flowui://docs/icons/atlas-capacity", kCapacityLimitation},
	{0x1107u, ErrorCode::PopupCapacityExceeded, ErrorSite::None, DevErrorAdviceCategory::Capacity,
		DevErrorAdvicePredicate::Always, 680u, DevErrorAdviceConfidence::Possible,
		"Popup capacity was exhausted", kCapacityExplanation, kCapacityAction,
		"popup.capacity", "flowui://docs/popups/capacity", kCapacityLimitation},

	// Lifecycle and storage protocol contracts.
	{0x1201u, ErrorCode::FrameAlreadyActive, ErrorSite::None, DevErrorAdviceCategory::Lifecycle,
		DevErrorAdvicePredicate::FrameContextAvailable, 720u, DevErrorAdviceConfidence::Strong,
		"Another window frame is already active", kFrameExplanation, kFrameAction,
		{}, "flowui://docs/frames/lifecycle", kFrameLimitation},
	{0x1202u, ErrorCode::FramePhaseViolation, ErrorSite::None, DevErrorAdviceCategory::Lifecycle,
		DevErrorAdvicePredicate::Always, 710u, DevErrorAdviceConfidence::Strong,
		"Frame API called in the wrong phase", kFrameExplanation, kFrameAction,
		{}, "flowui://docs/frames/lifecycle", kFrameLimitation},
	{0x1203u, ErrorCode::PreparedFrameStale, ErrorSite::None, DevErrorAdviceCategory::Lifecycle,
		DevErrorAdvicePredicate::Always, 710u, DevErrorAdviceConfidence::Strong,
		"Prepared frame evidence is stale", kFrameExplanation, kFrameAction,
		{}, "flowui://docs/frames/prepared-state", kFrameLimitation},
	{0x1204u, ErrorCode::StorageFrameProtocolViolation, ErrorSite::None, DevErrorAdviceCategory::Lifecycle,
		DevErrorAdvicePredicate::Always, 700u, DevErrorAdviceConfidence::Strong,
		"Storage frame protocol was violated", kFrameExplanation, kFrameAction,
		{}, "flowui://docs/storage/frame-protocol", kFrameLimitation},
	{0x1205u, ErrorCode::StorageSubmissionProtocolViolation, ErrorSite::None, DevErrorAdviceCategory::Lifecycle,
		DevErrorAdvicePredicate::Always, 700u, DevErrorAdviceConfidence::Strong,
		"Storage submission protocol was violated", kFrameExplanation, kFrameAction,
		{}, "flowui://docs/storage/submission-protocol", kFrameLimitation},

	// Resource identity and lifetime contracts.
	{0x1301u, ErrorCode::ResourceNotFound, ErrorSite::None, DevErrorAdviceCategory::Resource,
		DevErrorAdvicePredicate::Always, 650u, DevErrorAdviceConfidence::Possible,
		"Resource lookup found no live record", kResourceExplanation, kResourceAction,
		{}, "flowui://docs/resources/identity", kResourceLimitation},
	{0x1302u, ErrorCode::ResourceHandleInvalid, ErrorSite::None, DevErrorAdviceCategory::Resource,
		DevErrorAdvicePredicate::Always, 650u, DevErrorAdviceConfidence::Strong,
		"Resource handle is invalid or stale", kResourceExplanation, kResourceAction,
		{}, "flowui://docs/resources/handles", kResourceLimitation},
	{0x1303u, ErrorCode::ResourceKindMismatch, ErrorSite::None, DevErrorAdviceCategory::Resource,
		DevErrorAdvicePredicate::Always, 650u, DevErrorAdviceConfidence::Strong,
		"Resource kind does not match the operation", kResourceExplanation, kResourceAction,
		{}, "flowui://docs/resources/identity", kResourceLimitation},
	{0x1304u, ErrorCode::ResourceWindowMismatch, ErrorSite::None, DevErrorAdviceCategory::Resource,
		DevErrorAdvicePredicate::Always, 650u, DevErrorAdviceConfidence::Strong,
		"Resource belongs to a different window", kResourceExplanation, kResourceAction,
		{}, "flowui://docs/resources/window-ownership", kResourceLimitation},
	{0x1305u, ErrorCode::ResourcePublicationFailed, ErrorSite::None, DevErrorAdviceCategory::Resource,
		DevErrorAdvicePredicate::Always, 640u, DevErrorAdviceConfidence::Possible,
		"Resource publication did not complete", kResourceExplanation, kResourceAction,
		{}, "flowui://docs/resources/publication", kResourceLimitation},
	{0x1306u, ErrorCode::ResourceInUse, ErrorSite::None, DevErrorAdviceCategory::Resource,
		DevErrorAdvicePredicate::Always, 640u, DevErrorAdviceConfidence::Possible,
		"Resource is still owned by active work", kResourceExplanation, kResourceAction,
		{}, "flowui://docs/resources/lifetimes", kResourceLimitation},

	// Popup and viewport semantic contracts.
	{0x1401u, ErrorCode::PopupDuplicateSubmission, ErrorSite::None, DevErrorAdviceCategory::Popup,
		DevErrorAdvicePredicate::Always, 660u, DevErrorAdviceConfidence::Strong,
		"Popup identity was submitted more than once", kPopupExplanation, kPopupAction,
		"errors.policy.duplicatePopup", "flowui://docs/popups/submission", kPopupLimitation},
	{0x1402u, ErrorCode::PopupAnchorNotFound, ErrorSite::None, DevErrorAdviceCategory::Popup,
		DevErrorAdvicePredicate::Always, 650u, DevErrorAdviceConfidence::Possible,
		"Popup anchor was not present", kPopupExplanation, kPopupAction,
		{}, "flowui://docs/popups/anchors", kPopupLimitation},
	{0x1403u, ErrorCode::ViewportDetached, ErrorSite::None, DevErrorAdviceCategory::Viewport,
		DevErrorAdvicePredicate::Always, 650u, DevErrorAdviceConfidence::Strong,
		"Viewport was detached during use", kViewportExplanation, kViewportAction,
		{}, "flowui://docs/viewports/lifecycle", kViewportLimitation},
	{0x1404u, ErrorCode::ViewportNotFound, ErrorSite::None, DevErrorAdviceCategory::Viewport,
		DevErrorAdvicePredicate::Always, 640u, DevErrorAdviceConfidence::Possible,
		"Viewport identity was not found", kViewportExplanation, kViewportAction,
		"errors.policy.missingViewport", "flowui://docs/viewports/missing-policy", kViewportLimitation},
	{0x1405u, ErrorCode::ViewportGenerationIncomplete, ErrorSite::None, DevErrorAdviceCategory::Viewport,
		DevErrorAdvicePredicate::Always, 640u, DevErrorAdviceConfidence::Possible,
		"Viewport generation is incomplete", kViewportExplanation, kViewportAction,
		{}, "flowui://docs/viewports/generation", kViewportLimitation},
	{0x1406u, ErrorCode::ViewportRecordingFailed, ErrorSite::None, DevErrorAdviceCategory::Viewport,
		DevErrorAdvicePredicate::NativeCodeAvailable, 670u, DevErrorAdviceConfidence::Strong,
		"Viewport command recording returned a native failure", kViewportExplanation,
		kViewportAction, {}, "flowui://docs/viewports/recording", kViewportLimitation},

	// Asset contracts.
	{0x1501u, ErrorCode::AssetNotFound, ErrorSite::None, DevErrorAdviceCategory::Asset,
		DevErrorAdvicePredicate::Always, 620u, DevErrorAdviceConfidence::Possible,
		"Asset path did not resolve to a file", kAssetExplanation, kAssetAction,
		{}, "flowui://docs/assets/paths", kAssetLimitation},
	{0x1502u, ErrorCode::AssetOpenFailed, ErrorSite::None, DevErrorAdviceCategory::Asset,
		DevErrorAdvicePredicate::Always, 620u, DevErrorAdviceConfidence::Possible,
		"Asset file could not be opened", kAssetExplanation, kAssetAction,
		{}, "flowui://docs/assets/io", kAssetLimitation},
	{0x1503u, ErrorCode::AssetReadFailed, ErrorSite::None, DevErrorAdviceCategory::Asset,
		DevErrorAdvicePredicate::Always, 620u, DevErrorAdviceConfidence::Possible,
		"Asset file could not be read completely", kAssetExplanation, kAssetAction,
		{}, "flowui://docs/assets/io", kAssetLimitation},
	{0x1504u, ErrorCode::AssetFormatUnsupported, ErrorSite::None, DevErrorAdviceCategory::Asset,
		DevErrorAdvicePredicate::Always, 630u, DevErrorAdviceConfidence::Strong,
		"Asset format is unsupported", kAssetExplanation, kAssetAction,
		{}, "flowui://docs/assets/formats", kAssetLimitation},
	{0x1505u, ErrorCode::AssetPayloadInvalid, ErrorSite::None, DevErrorAdviceCategory::Asset,
		DevErrorAdvicePredicate::Always, 630u, DevErrorAdviceConfidence::Strong,
		"Asset payload failed validation", kAssetExplanation, kAssetAction,
		{}, "flowui://docs/assets/validation", kAssetLimitation},
	{0x1506u, ErrorCode::AssetDecodeFailed, ErrorSite::None, DevErrorAdviceCategory::Asset,
		DevErrorAdvicePredicate::Always, 620u, DevErrorAdviceConfidence::Possible,
		"Asset decoding failed", kAssetExplanation, kAssetAction,
		{}, "flowui://docs/assets/decoding", kAssetLimitation},

	// Renderer contracts.
	{0x1601u, ErrorCode::RendererConfigurationInvalid, ErrorSite::None, DevErrorAdviceCategory::Renderer,
		DevErrorAdvicePredicate::Always, 650u, DevErrorAdviceConfidence::Strong,
		"Renderer configuration is invalid", kRendererExplanation, kRendererAction,
		"renderer", "flowui://docs/renderer/configuration", kRendererLimitation},
	{0x1602u, ErrorCode::RenderCommandInvalid, ErrorSite::None, DevErrorAdviceCategory::Renderer,
		DevErrorAdvicePredicate::FrameContextAvailable, 670u, DevErrorAdviceConfidence::Strong,
		"Frame contains an invalid render command", kRendererExplanation, kRendererAction,
		{}, "flowui://docs/renderer/commands", kRendererLimitation},
	{0x1603u, ErrorCode::RendererGenerationStale, ErrorSite::None, DevErrorAdviceCategory::Renderer,
		DevErrorAdvicePredicate::Always, 660u, DevErrorAdviceConfidence::Strong,
		"Renderer generation is stale", kRendererExplanation, kRendererAction,
		{}, "flowui://docs/renderer/generations", kRendererLimitation},
	{0x1604u, ErrorCode::RendererNativeResourceInvalid, ErrorSite::None, DevErrorAdviceCategory::Renderer,
		DevErrorAdvicePredicate::NativeCodeAvailable, 670u, DevErrorAdviceConfidence::Strong,
		"Renderer native resource is invalid", kRendererExplanation, kRendererAction,
		{}, "flowui://docs/renderer/native-resources", kRendererLimitation},

	// Vulkan capability and native-operation contracts.
	{0x1701u, ErrorCode::VulkanVersionUnsupported, ErrorSite::None, DevErrorAdviceCategory::Vulkan,
		DevErrorAdvicePredicate::Always, 760u, DevErrorAdviceConfidence::Certain,
		"Required Vulkan version is unavailable", kVulkanCapabilityExplanation,
		kVulkanCapabilityAction, "vk.apiVersion", "flowui://docs/vulkan/requirements", kVulkanLimitation},
	{0x1702u, ErrorCode::VulkanExtensionMissing, ErrorSite::None, DevErrorAdviceCategory::Vulkan,
		DevErrorAdvicePredicate::Always, 750u, DevErrorAdviceConfidence::Strong,
		"Required Vulkan extension is unavailable", kVulkanCapabilityExplanation,
		kVulkanCapabilityAction, "vk.extensions", "flowui://docs/vulkan/extensions", kVulkanLimitation},
	{0x1703u, ErrorCode::VulkanDeviceUnavailable, ErrorSite::None, DevErrorAdviceCategory::Vulkan,
		DevErrorAdvicePredicate::Always, 750u, DevErrorAdviceConfidence::Strong,
		"No compatible Vulkan device was selected", kVulkanCapabilityExplanation,
		kVulkanCapabilityAction, "vk.device", "flowui://docs/vulkan/device-selection", kVulkanLimitation},
	{0x1704u, ErrorCode::VulkanFeatureMissing, ErrorSite::None, DevErrorAdviceCategory::Vulkan,
		DevErrorAdvicePredicate::Always, 750u, DevErrorAdviceConfidence::Strong,
		"Required Vulkan feature is unavailable", kVulkanCapabilityExplanation,
		kVulkanCapabilityAction, "vk.features", "flowui://docs/vulkan/features", kVulkanLimitation},
	{0x1705u, ErrorCode::VulkanNativeCallFailed, ErrorSite::None, DevErrorAdviceCategory::Vulkan,
		DevErrorAdvicePredicate::NativeCodeAvailable, 780u, DevErrorAdviceConfidence::Strong,
		"Vulkan call returned an error", kVulkanNativeExplanation, kVulkanNativeAction,
		{}, "flowui://docs/vulkan/native-errors", kVulkanLimitation},
	{0x1706u, ErrorCode::VulkanDeviceLost, ErrorSite::None, DevErrorAdviceCategory::Vulkan,
		DevErrorAdvicePredicate::NativeCodeAvailable, 900u, DevErrorAdviceConfidence::Strong,
		"Vulkan device was lost", kVulkanNativeExplanation,
		"Preserve the captured submission, validation, and memory evidence; inspect device-fault support before attempting application-level recovery.",
		{}, "flowui://docs/vulkan/device-loss", kVulkanLimitation},
	{0x1707u, ErrorCode::PresentationFailed, ErrorSite::None, DevErrorAdviceCategory::Vulkan,
		DevErrorAdvicePredicate::NativeCodeAvailable, 760u, DevErrorAdviceConfidence::Strong,
		"Swapchain presentation failed", kVulkanNativeExplanation, kVulkanNativeAction,
		{}, "flowui://docs/vulkan/presentation", kVulkanLimitation},

	// Resolution and capture-quality guidance may accompany a contract-specific result.
	{0x1801u, ErrorCode::None, ErrorSite::None, DevErrorAdviceCategory::Lifecycle,
		DevErrorAdvicePredicate::FallbackApplied, 300u, DevErrorAdviceConfidence::Certain,
		kFallbackTitle, kFallbackExplanation, kFallbackAction, {},
		"flowui://docs/errors/resolutions", kFallbackLimitation},
	{0x1802u, ErrorCode::None, ErrorSite::None, DevErrorAdviceCategory::CaptureQuality,
		DevErrorAdvicePredicate::CaptureIncomplete, 200u, DevErrorAdviceConfidence::InsufficientEvidence,
		kCaptureTitle, kCaptureExplanation, kCaptureAction, "FLOWUI_DEV_ERROR_LEVEL",
		"flowui://docs/dev/errors/capture-levels", kCaptureLimitation},
});

constexpr auto kNoGuidance = std::to_array<DevErrorNoGuidanceAnnotation>({
	{ErrorCode::InternalInvariantBroken, ErrorCode::ForeignExceptionObserved,
		"Internal/common contracts currently rely on their production descriptor and captured evidence."},
	{ErrorCode::AppUnavailable, ErrorCode::AppTickSpaceExhausted,
		"Application lifecycle wording is deferred to a dedicated advice pass; fatal evidence is preserved by the completed capsule path."},
	{ErrorCode::PlatformInitializationFailed, ErrorCode::PlatformInputQueueOverflow,
		"Platform-specific remediation depends on backend evidence and is not yet catalogued."},
	{ErrorCode::InvalidWindowId, ErrorCode::WindowRecreationFailed,
		"Window contracts are currently explained by lifecycle evidence without additional advice prose."},
	{ErrorCode::FrameAlreadyActive, ErrorCode::FrameCapacityExceeded,
		"Covered frame contracts coexist with an explicit annotation for the remaining frame codes."},
	{ErrorCode::VulkanVersionUnsupported, ErrorCode::PresentIdSpaceExhausted,
		"Covered Vulkan contracts coexist with an explicit annotation for remaining swapchain cases."},
	{ErrorCode::ShaderUnavailable, ErrorCode::RendererNativeResourceInvalid,
		"Covered renderer contracts coexist with an explicit annotation for shader-specific cases."},
	{ErrorCode::StorageConfigurationInvalid, ErrorCode::StorageGenerationExhausted,
		"Covered storage contracts coexist with an explicit annotation for remaining storage cases."},
	{ErrorCode::InvalidResourceKey, ErrorCode::ResourceGenerationExhausted,
		"Covered resource contracts coexist with an explicit annotation for remaining identity cases."},
	{ErrorCode::AssetPathEmpty, ErrorCode::AssetDecodeFailed,
		"Covered asset contracts coexist with an explicit annotation for an empty caller path."},
	{ErrorCode::FontFamilyAlreadyExists, ErrorCode::DefaultFontUnavailable,
		"Font-specific guidance beyond atlas capacity is intentionally deferred."},
	{ErrorCode::ImageDecodeFailed, ErrorCode::ImagePublicationFailed,
		"Image-specific guidance currently uses the general asset and resource evidence."},
	{ErrorCode::IconSourceInvalid, ErrorCode::IconKeyCollision,
		"Icon-specific guidance beyond atlas capacity is intentionally deferred."},
	{ErrorCode::InvalidElementId, ErrorCode::ElementStorageStale,
		"Element semantic guidance will be expanded with DevTooling source scopes."},
	{ErrorCode::InvalidActionId, ErrorCode::ActionPublicationConflict,
		"Action-specific guidance will be expanded with DevTooling callback scopes."},
	{ErrorCode::ThemeTypeNotFound, ErrorCode::ThemeMutationFailed,
		"Theme-specific guidance is intentionally deferred."},
	{ErrorCode::InputFieldUnavailable, ErrorCode::InputSizeLimitExceeded,
		"Input-specific guidance is intentionally deferred."},
	{ErrorCode::PopupFrameInactive, ErrorCode::PopupCapacityExceeded,
		"Covered popup contracts coexist with an explicit annotation for frame-inactive behavior."},
	{ErrorCode::ShortcutInvalid, ErrorCode::ShortcutNotFound,
		"Shortcut-specific guidance is intentionally deferred."},
	{ErrorCode::ViewportDetached, ErrorCode::ViewportRecordingFailed,
		"Covered viewport contracts coexist with an explicit annotation for remaining viewport cases."},
});

} // namespace

std::span<const DevErrorAdviceDescriptor> devErrorAdviceCatalogue() noexcept {
	return kCatalogue;
}

std::span<const DevErrorNoGuidanceAnnotation> devErrorNoGuidanceAnnotations() noexcept {
	return kNoGuidance;
}

bool devErrorAdviceCatalogueCovers(ErrorCode code) noexcept {
	if (code == ErrorCode::None) return true;
	if (std::any_of(kCatalogue.begin(), kCatalogue.end(), [&](const auto& descriptor) {
		return descriptor.code == code;
	})) return true;
	const uint16_t value = static_cast<uint16_t>(code);
	return std::any_of(kNoGuidance.begin(), kNoGuidance.end(), [&](const auto& annotation) {
		return value >= static_cast<uint16_t>(annotation.first) &&
			value <= static_cast<uint16_t>(annotation.last);
	});
}

} // namespace FlowUi::devSystems

#endif
