#pragma once

#include "FlowUi/Private/ErrorSupport.hpp"
#include "FlowUi/Public/errorManagement/ErrorCodeTypes.hpp"

namespace FlowUi {

/**
 * Return static catalog metadata for code. Unknown values retain the numeric code and receive a fatal
 * process-boundary descriptor named UnknownErrorCode.
 */
[[nodiscard]] constexpr ErrorDescriptor describeError(ErrorCode code) noexcept {
	using detail::localDescriptor;
	switch (code) {
	case ErrorCode::None:
		return {};

	case ErrorCode::InternalInvariantBroken:
		return {code,
				ErrorDomain::Core,
				ErrorCategory::Fatal,
				ErrorBoundary::Process,
				ErrorResolvability::NotResolvable,
				ErrorImpact::Fatal,
				ErrorResolutionPolicy::None,
				ErrorResolution::Terminated,
				"InternalInvariantBroken",
				"A required FlowUi internal invariant was broken."};
	case ErrorCode::IdentitySpaceExhausted:
		return {code,
				ErrorDomain::Core,
				ErrorCategory::Fatal,
				ErrorBoundary::Process,
				ErrorResolvability::NotResolvable,
				ErrorImpact::Fatal,
				ErrorResolutionPolicy::None,
				ErrorResolution::Terminated,
				"IdentitySpaceExhausted",
				"A non-reusable FlowUi identity space was exhausted."};
	case ErrorCode::ArithmeticOverflow:
		return localDescriptor(code, "ArithmeticOverflow",
							   "A FlowUi size or offset calculation overflowed.");
	case ErrorCode::AllocationFailed:
		return {code,
				ErrorDomain::Core,
				ErrorCategory::Unwind,
				ErrorBoundary::AppLifecycle,
				ErrorResolvability::RecreateApp,
				ErrorImpact::AppUnavailable,
				ErrorResolutionPolicy::CallerDefined,
				ErrorResolution::Propagated,
				"AllocationFailed",
				"FlowUi could not allocate required memory."};
	case ErrorCode::ObjectNotInitialized:
		return localDescriptor(code, "ObjectNotInitialized",
							   "The FlowUi object is not initialized.");
	case ErrorCode::ObjectAlreadyInitialized:
		return localDescriptor(code, "ObjectAlreadyInitialized",
							   "The FlowUi object is already initialized.");
	case ErrorCode::UnsupportedBuildFeature:
		return {code,
				ErrorDomain::Core,
				ErrorCategory::Static,
				ErrorBoundary::CompileExpression,
				ErrorResolvability::NotResolvable,
				ErrorImpact::OperationFailed,
				ErrorResolutionPolicy::None,
				ErrorResolution::Rejected,
				"UnsupportedBuildFeature",
				"The requested FlowUi feature is not present in this build."};
	case ErrorCode::WrongThread:
		return localDescriptor(code, "WrongThread",
							   "The operation was called from the wrong thread.");
	case ErrorCode::CleanupIncomplete:
		return {code,
				ErrorDomain::Core,
				ErrorCategory::Fatal,
				ErrorBoundary::Process,
				ErrorResolvability::NotResolvable,
				ErrorImpact::AppUnavailable,
				ErrorResolutionPolicy::HardCoded,
				ErrorResolution::Terminated,
				"CleanupIncomplete",
				"FlowUi could not complete required cleanup."};
	case ErrorCode::ForeignExceptionObserved:
		return {code,
				ErrorDomain::Core,
				ErrorCategory::Unwind,
				ErrorBoundary::AppLifecycle,
				ErrorResolvability::CallerResolvable,
				ErrorImpact::OperationFailed,
				ErrorResolutionPolicy::CallerDefined,
				ErrorResolution::Propagated,
				"ForeignExceptionObserved",
				"User or third-party code threw while FlowUi was active."};

	case ErrorCode::AppUnavailable:
		return localDescriptor(code, "AppUnavailable", "The FlowUi application is unavailable.");
	case ErrorCode::AppInitializationFailed:
		return {code,
				ErrorDomain::App,
				ErrorCategory::Unwind,
				ErrorBoundary::AppLifecycle,
				ErrorResolvability::CallerResolvable,
				ErrorImpact::AppUnavailable,
				ErrorResolutionPolicy::CallerDefined,
				ErrorResolution::Propagated,
				"AppInitializationFailed",
				"FlowUi application initialization failed."};
	case ErrorCode::AppTickSpaceExhausted:
		return {code,
				ErrorDomain::App,
				ErrorCategory::Fatal,
				ErrorBoundary::Process,
				ErrorResolvability::NotResolvable,
				ErrorImpact::Fatal,
				ErrorResolutionPolicy::None,
				ErrorResolution::Terminated,
				"AppTickSpaceExhausted",
				"The FlowUi application tick identity space was exhausted."};

	case ErrorCode::PlatformInitializationFailed:
		return {code,
				ErrorDomain::Platform,
				ErrorCategory::Unwind,
				ErrorBoundary::AppLifecycle,
				ErrorResolvability::CallerResolvable,
				ErrorImpact::AppUnavailable,
				ErrorResolutionPolicy::CallerDefined,
				ErrorResolution::Propagated,
				"PlatformInitializationFailed",
				"The platform window system could not be initialized."};
	case ErrorCode::PlatformMonitorUnavailable:
		return localDescriptor(code, "PlatformMonitorUnavailable",
							   "The requested platform monitor is unavailable.");
	case ErrorCode::PlatformRequiredExtensionMissing:
		return {code,
				ErrorDomain::Platform,
				ErrorCategory::Unwind,
				ErrorBoundary::AppLifecycle,
				ErrorResolvability::CallerResolvable,
				ErrorImpact::AppUnavailable,
				ErrorResolutionPolicy::CallerDefined,
				ErrorResolution::Propagated,
				"PlatformRequiredExtensionMissing",
				"A required platform extension is unavailable."};
	case ErrorCode::PlatformSurfaceCreationFailed:
		return {code,
				ErrorDomain::Platform,
				ErrorCategory::Unwind,
				ErrorBoundary::WindowLifecycle,
				ErrorResolvability::CallerResolvable,
				ErrorImpact::WindowUnavailable,
				ErrorResolutionPolicy::CallerDefined,
				ErrorResolution::Propagated,
				"PlatformSurfaceCreationFailed",
				"The platform rendering surface could not be created."};
	case ErrorCode::PlatformInputQueueOverflow:
		return {code,
				ErrorDomain::Platform,
				ErrorCategory::Local,
				ErrorBoundary::ManagerOperation,
				ErrorResolvability::AutomaticallyResolved,
				ErrorImpact::Degraded,
				ErrorResolutionPolicy::AppConfigured,
				ErrorResolution::Ignored,
				"PlatformInputQueueOverflow",
				"The bounded platform input queue overflowed."};

	case ErrorCode::InvalidWindowId:
		return localDescriptor(code, "InvalidWindowId",
							   "The window identity is invalid or no longer registered.");
	case ErrorCode::InvalidWindowConfiguration:
		return localDescriptor(code, "InvalidWindowConfiguration",
							   "The window configuration is invalid.");
	case ErrorCode::WindowCreationFailed:
		return localDescriptor(code, "WindowCreationFailed",
							   "The FlowUi window could not be created.",
							   ErrorBoundary::WindowLifecycle);
	case ErrorCode::WindowIdSpaceExhausted:
	case ErrorCode::WindowRegistryCollision:
		return {code,
				ErrorDomain::Window,
				ErrorCategory::Fatal,
				ErrorBoundary::Process,
				ErrorResolvability::NotResolvable,
				ErrorImpact::Fatal,
				ErrorResolutionPolicy::None,
				ErrorResolution::Terminated,
				code == ErrorCode::WindowIdSpaceExhausted ? "WindowIdSpaceExhausted"
														  : "WindowRegistryCollision",
				code == ErrorCode::WindowIdSpaceExhausted
					? "The FlowUi window identity space was exhausted."
					: "The FlowUi window registry detected an identity collision."};
	case ErrorCode::MainWindowDestructionForbidden:
		return localDescriptor(code, "MainWindowDestructionForbidden",
							   "The semantic main window cannot be destroyed explicitly.");
	case ErrorCode::WindowPresentationUnsupported:
		return localDescriptor(code, "WindowPresentationUnsupported",
							   "The selected device cannot present to this window.",
							   ErrorBoundary::WindowLifecycle);
	case ErrorCode::WindowRecreationFailed:
		return {code,
				ErrorDomain::Window,
				ErrorCategory::Unwind,
				ErrorBoundary::WindowLifecycle,
				ErrorResolvability::RecreateWindow,
				ErrorImpact::WindowUnavailable,
				ErrorResolutionPolicy::CallerDefined,
				ErrorResolution::Propagated,
				"WindowRecreationFailed",
				"The FlowUi window rendering resources could not be recreated."};

	case ErrorCode::FrameAlreadyActive:
		return localDescriptor(code, "FrameAlreadyActive",
							   "Another FlowUi window frame is already active.");
	case ErrorCode::FramePhaseViolation:
		return localDescriptor(code, "FramePhaseViolation",
							   "The frame operation is invalid in the current phase.");
	case ErrorCode::FrameNotReady:
		return localDescriptor(code, "FrameNotReady",
							   "The FlowUi window is not ready for the requested frame operation.");
	case ErrorCode::FrameNumberSpaceExhausted:
		return {code,
				ErrorDomain::Frame,
				ErrorCategory::Fatal,
				ErrorBoundary::Process,
				ErrorResolvability::NotResolvable,
				ErrorImpact::Fatal,
				ErrorResolutionPolicy::None,
				ErrorResolution::Terminated,
				"FrameNumberSpaceExhausted",
				"The FlowUi frame identity space was exhausted."};
	case ErrorCode::PreparedFrameStale:
		return {code,
				ErrorDomain::Frame,
				ErrorCategory::Fatal,
				ErrorBoundary::WindowFrame,
				ErrorResolvability::NotResolvable,
				ErrorImpact::Fatal,
				ErrorResolutionPolicy::None,
				ErrorResolution::Halted,
				"PreparedFrameStale",
				"Prepared UI data does not match its sealed storage frame."};
	case ErrorCode::FrameCapacityExceeded:
		return {code,
				ErrorDomain::Frame,
				ErrorCategory::Unwind,
				ErrorBoundary::WindowFrame,
				ErrorResolvability::CallerResolvable,
				ErrorImpact::FrameCanceled,
				ErrorResolutionPolicy::CallerDefined,
				ErrorResolution::Propagated,
				"FrameCapacityExceeded",
				"The frame exceeded a configured FlowUi capacity."};
	case ErrorCode::UiBuildCallbackFailed:
		return {code,
				ErrorDomain::Frame,
				ErrorCategory::Local,
				ErrorBoundary::WindowFrame,
				ErrorResolvability::AutomaticallyResolved,
				ErrorImpact::FrameCanceled,
				ErrorResolutionPolicy::HardCoded,
				ErrorResolution::CanceledFrame,
				"UiBuildCallbackFailed",
				"A managed window UI build callback threw an exception."};

	case ErrorCode::VulkanVersionUnsupported:
	case ErrorCode::VulkanExtensionMissing:
	case ErrorCode::VulkanDeviceUnavailable:
	case ErrorCode::VulkanFeatureMissing:
		return {code,
				ErrorDomain::Vulkan,
				ErrorCategory::Unwind,
				ErrorBoundary::AppLifecycle,
				ErrorResolvability::CallerResolvable,
				ErrorImpact::AppUnavailable,
				ErrorResolutionPolicy::CallerDefined,
				ErrorResolution::Propagated,
				code == ErrorCode::VulkanVersionUnsupported
					? "VulkanVersionUnsupported"
					: (code == ErrorCode::VulkanExtensionMissing
						   ? "VulkanExtensionMissing"
						   : (code == ErrorCode::VulkanDeviceUnavailable ? "VulkanDeviceUnavailable"
																		 : "VulkanFeatureMissing")),
				code == ErrorCode::VulkanVersionUnsupported
					? "The required Vulkan version is unavailable."
					: (code == ErrorCode::VulkanExtensionMissing
						   ? "A required Vulkan extension is unavailable."
						   : (code == ErrorCode::VulkanDeviceUnavailable
								  ? "No compatible Vulkan device is available."
								  : "The selected Vulkan device lacks a required feature."))};
	case ErrorCode::VulkanNativeCallFailed:
		return {code,
				ErrorDomain::Vulkan,
				ErrorCategory::Unwind,
				ErrorBoundary::WindowFrame,
				ErrorResolvability::CallerResolvable,
				ErrorImpact::FrameCanceled,
				ErrorResolutionPolicy::CallerDefined,
				ErrorResolution::Propagated,
				"VulkanNativeCallFailed",
				"A Vulkan operation failed."};
	case ErrorCode::VulkanDeviceLost:
		return {code,
				ErrorDomain::Vulkan,
				ErrorCategory::Unwind,
				ErrorBoundary::AppLifecycle,
				ErrorResolvability::RecreateApp,
				ErrorImpact::AppUnavailable,
				ErrorResolutionPolicy::CallerDefined,
				ErrorResolution::Propagated,
				"VulkanDeviceLost",
				"The Vulkan device was lost."};
	case ErrorCode::SwapchainUnavailable:
		return localDescriptor(code, "SwapchainUnavailable",
							   "A usable swapchain could not be created.",
							   ErrorBoundary::WindowLifecycle);
	case ErrorCode::SwapchainOutOfDate:
		return {code,
				ErrorDomain::Vulkan,
				ErrorCategory::Local,
				ErrorBoundary::WindowLifecycle,
				ErrorResolvability::AutomaticallyResolved,
				ErrorImpact::FrameCanceled,
				ErrorResolutionPolicy::HardCoded,
				ErrorResolution::RecreatedWindow,
				"SwapchainOutOfDate",
				"The window swapchain is out of date."};
	case ErrorCode::SwapchainImageInvalid:
		return {code,
				ErrorDomain::Vulkan,
				ErrorCategory::Fatal,
				ErrorBoundary::Process,
				ErrorResolvability::NotResolvable,
				ErrorImpact::Fatal,
				ErrorResolutionPolicy::None,
				ErrorResolution::Terminated,
				"SwapchainImageInvalid",
				"The acquired swapchain image is inconsistent with FlowUi state."};
	case ErrorCode::PresentationFailed:
		return {code,
				ErrorDomain::Vulkan,
				ErrorCategory::Unwind,
				ErrorBoundary::WindowLifecycle,
				ErrorResolvability::RecreateWindow,
				ErrorImpact::WindowUnavailable,
				ErrorResolutionPolicy::CallerDefined,
				ErrorResolution::Propagated,
				"PresentationFailed",
				"Presentation of a FlowUi window failed."};
	case ErrorCode::PresentIdSpaceExhausted:
		return {code,
				ErrorDomain::Vulkan,
				ErrorCategory::Fatal,
				ErrorBoundary::Process,
				ErrorResolvability::NotResolvable,
				ErrorImpact::Fatal,
				ErrorResolutionPolicy::None,
				ErrorResolution::Terminated,
				"PresentIdSpaceExhausted",
				"The swapchain presentation identity space was exhausted."};

	case ErrorCode::ShaderUnavailable:
	case ErrorCode::ShaderInvalid:
	case ErrorCode::RendererConfigurationInvalid:
		return {code,
				ErrorDomain::Renderer,
				ErrorCategory::Unwind,
				ErrorBoundary::AppLifecycle,
				ErrorResolvability::CallerResolvable,
				ErrorImpact::AppUnavailable,
				ErrorResolutionPolicy::CallerDefined,
				ErrorResolution::Propagated,
				code == ErrorCode::ShaderUnavailable
					? "ShaderUnavailable"
					: (code == ErrorCode::ShaderInvalid ? "ShaderInvalid"
														: "RendererConfigurationInvalid"),
				code == ErrorCode::ShaderUnavailable
					? "A required renderer shader is unavailable."
					: (code == ErrorCode::ShaderInvalid
						   ? "A renderer shader payload is invalid."
						   : "The renderer configuration is invalid.")};
	case ErrorCode::RendererCapacityExceeded:
		return {code,
				ErrorDomain::Renderer,
				ErrorCategory::Unwind,
				ErrorBoundary::WindowFrame,
				ErrorResolvability::CallerResolvable,
				ErrorImpact::FrameCanceled,
				ErrorResolutionPolicy::CallerDefined,
				ErrorResolution::Propagated,
				"RendererCapacityExceeded",
				"The renderer exceeded a configured capacity."};
	case ErrorCode::RenderCommandInvalid:
		return {code,
				ErrorDomain::Renderer,
				ErrorCategory::Unwind,
				ErrorBoundary::WindowFrame,
				ErrorResolvability::CallerResolvable,
				ErrorImpact::FrameCanceled,
				ErrorResolutionPolicy::CallerDefined,
				ErrorResolution::Propagated,
				"RenderCommandInvalid",
				"The UI render command stream is invalid."};
	case ErrorCode::RendererGenerationStale:
	case ErrorCode::RendererNativeResourceInvalid:
		return {code,
				ErrorDomain::Renderer,
				ErrorCategory::Fatal,
				ErrorBoundary::Process,
				ErrorResolvability::NotResolvable,
				ErrorImpact::Fatal,
				ErrorResolutionPolicy::None,
				ErrorResolution::Terminated,
				code == ErrorCode::RendererGenerationStale ? "RendererGenerationStale"
														   : "RendererNativeResourceInvalid",
				code == ErrorCode::RendererGenerationStale
					? "The renderer received a stale resource generation."
					: "The renderer received invalid committed native resources."};

	case ErrorCode::StorageConfigurationInvalid:
		return localDescriptor(code, "StorageConfigurationInvalid",
							   "The storage configuration is invalid.");
	case ErrorCode::StorageBudgetExceeded:
	case ErrorCode::StorageCapacityExceeded:
		return {code,
				ErrorDomain::Storage,
				ErrorCategory::Local,
				ErrorBoundary::DirectCaller,
				ErrorResolvability::CallerResolvable,
				ErrorImpact::OperationFailed,
				ErrorResolutionPolicy::AppConfigured,
				ErrorResolution::Rejected,
				code == ErrorCode::StorageBudgetExceeded ? "StorageBudgetExceeded"
														 : "StorageCapacityExceeded",
				code == ErrorCode::StorageBudgetExceeded
					? "A FlowUi storage budget was exceeded."
					: "A FlowUi storage capacity was exceeded."};
	case ErrorCode::StorageMutationSealed:
		return localDescriptor(
			code, "StorageMutationSealed",
			"Storage mutation is not allowed while a sealed snapshot is active.");
	case ErrorCode::StorageFrameProtocolViolation:
		return localDescriptor(code, "StorageFrameProtocolViolation",
							   "The storage frame operation violates the current frame state.",
							   ErrorBoundary::WindowFrame);
	case ErrorCode::StorageSubmissionProtocolViolation:
		return {code,
				ErrorDomain::Storage,
				ErrorCategory::Fatal,
				ErrorBoundary::Process,
				ErrorResolvability::NotResolvable,
				ErrorImpact::Fatal,
				ErrorResolutionPolicy::None,
				ErrorResolution::Terminated,
				"StorageSubmissionProtocolViolation",
				"The storage submission ownership protocol was violated."};
	case ErrorCode::StoragePublicationFailed:
		return localDescriptor(code, "StoragePublicationFailed",
							   "Storage could not publish the requested record or resource.");
	case ErrorCode::StorageGenerationExhausted:
		return {code,
				ErrorDomain::Storage,
				ErrorCategory::Fatal,
				ErrorBoundary::Process,
				ErrorResolvability::NotResolvable,
				ErrorImpact::Fatal,
				ErrorResolutionPolicy::None,
				ErrorResolution::Terminated,
				"StorageGenerationExhausted",
				"A storage generation space was exhausted."};

	case ErrorCode::InvalidResourceKey:
		return localDescriptor(code, "InvalidResourceKey",
							   "The resource key is invalid for this manager.");
	case ErrorCode::ResourceNotFound:
		return localDescriptor(code, "ResourceNotFound",
							   "The requested FlowUi resource was not found.");
	case ErrorCode::ResourceHandleInvalid:
		return localDescriptor(code, "ResourceHandleInvalid",
							   "The FlowUi resource handle is invalid or stale.");
	case ErrorCode::ResourceKindMismatch:
		return localDescriptor(code, "ResourceKindMismatch",
							   "The FlowUi resource has the wrong kind.");
	case ErrorCode::ResourceWindowMismatch:
		return localDescriptor(code, "ResourceWindowMismatch",
							   "The FlowUi resource belongs to a different window.");
	case ErrorCode::ResourceCreationFailed:
		return localDescriptor(code, "ResourceCreationFailed",
							   "The FlowUi resource could not be created.");
	case ErrorCode::ResourcePublicationFailed:
		return localDescriptor(code, "ResourcePublicationFailed",
							   "The FlowUi resource could not be published.");
	case ErrorCode::ResourceInUse:
		return localDescriptor(code, "ResourceInUse", "The FlowUi resource is still in use.");
	case ErrorCode::ResourceGenerationExhausted:
		return {code,
				ErrorDomain::Resource,
				ErrorCategory::Fatal,
				ErrorBoundary::Process,
				ErrorResolvability::NotResolvable,
				ErrorImpact::Fatal,
				ErrorResolutionPolicy::None,
				ErrorResolution::Terminated,
				"ResourceGenerationExhausted",
				"A resource generation space was exhausted."};

	case ErrorCode::AssetPathEmpty:
		return localDescriptor(code, "AssetPathEmpty", "The asset path is empty.");
	case ErrorCode::AssetNotFound:
		return localDescriptor(code, "AssetNotFound", "The requested asset does not exist.");
	case ErrorCode::AssetOpenFailed:
		return localDescriptor(code, "AssetOpenFailed", "The asset could not be opened.");
	case ErrorCode::AssetReadFailed:
		return localDescriptor(code, "AssetReadFailed", "The asset could not be read.");
	case ErrorCode::AssetFormatUnsupported:
		return localDescriptor(code, "AssetFormatUnsupported", "The asset format is unsupported.");
	case ErrorCode::AssetPayloadInvalid:
		return localDescriptor(code, "AssetPayloadInvalid", "The asset payload is invalid.");
	case ErrorCode::AssetDecodeFailed:
		return localDescriptor(code, "AssetDecodeFailed", "The asset could not be decoded.");

	case ErrorCode::FontFamilyAlreadyExists:
		return localDescriptor(code, "FontFamilyAlreadyExists",
							   "The font family is already registered.");
	case ErrorCode::FontFamilyNotFound:
		return localDescriptor(code, "FontFamilyNotFound", "The font family is not registered.");
	case ErrorCode::FontBakeUnavailable:
		return localDescriptor(code, "FontBakeUnavailable",
							   "Runtime font baking is unavailable in this build.");
	case ErrorCode::FontBakeFailed:
		return localDescriptor(code, "FontBakeFailed", "Runtime font baking failed.");
	case ErrorCode::FontAtlasCapacityExceeded:
		return {code,
				ErrorDomain::Font,
				ErrorCategory::Local,
				ErrorBoundary::ManagerOperation,
				ErrorResolvability::CallerResolvable,
				ErrorImpact::OperationFailed,
				ErrorResolutionPolicy::AppConfigured,
				ErrorResolution::Rejected,
				"FontAtlasCapacityExceeded",
				"The font atlas capacity was exceeded."};
	case ErrorCode::FontIdSpaceExhausted:
		return localDescriptor(code, "FontIdSpaceExhausted",
							   "The font identity space was exhausted.",
							   ErrorBoundary::ManagerOperation);
	case ErrorCode::DefaultFontUnavailable:
		return {code,
				ErrorDomain::Font,
				ErrorCategory::Local,
				ErrorBoundary::ManagerOperation,
				ErrorResolvability::CallerResolvable,
				ErrorImpact::Degraded,
				ErrorResolutionPolicy::AppConfigured,
				ErrorResolution::UsedFallback,
				"DefaultFontUnavailable",
				"No configured default font could be loaded."};

	case ErrorCode::ImageDecodeFailed:
		return localDescriptor(code, "ImageDecodeFailed", "The image could not be decoded.");
	case ErrorCode::ImageSizeOverflow:
		return localDescriptor(code, "ImageSizeOverflow",
							   "The decoded image is too large to address.");
	case ErrorCode::ImagePublicationFailed:
		return localDescriptor(code, "ImagePublicationFailed",
							   "The decoded image could not be published.");

	case ErrorCode::IconSourceInvalid:
		return localDescriptor(code, "IconSourceInvalid", "The icon source is invalid.");
	case ErrorCode::IconNotFound:
		return localDescriptor(code, "IconNotFound", "The requested icon is not registered.");
	case ErrorCode::IconRasterizationFailed:
		return localDescriptor(code, "IconRasterizationFailed",
							   "The icon could not be rasterized.");
	case ErrorCode::IconRasterInvalid:
		return localDescriptor(code, "IconRasterInvalid", "The icon raster payload is invalid.");
	case ErrorCode::IconAtlasCapacityExceeded:
		return {code,
				ErrorDomain::Icon,
				ErrorCategory::Local,
				ErrorBoundary::ManagerOperation,
				ErrorResolvability::CallerResolvable,
				ErrorImpact::Degraded,
				ErrorResolutionPolicy::AppConfigured,
				ErrorResolution::UsedFallback,
				"IconAtlasCapacityExceeded",
				"The icon atlas has no usable capacity."};
	case ErrorCode::IconKeyCollision:
		return {code,
				ErrorDomain::Icon,
				ErrorCategory::Fatal,
				ErrorBoundary::Process,
				ErrorResolvability::NotResolvable,
				ErrorImpact::Fatal,
				ErrorResolutionPolicy::None,
				ErrorResolution::Terminated,
				"IconKeyCollision",
				"An internally generated icon key collided."};

	case ErrorCode::InvalidElementId:
		return localDescriptor(code, "InvalidElementId", "The Flow element identity is invalid.");
	case ErrorCode::ElementDefinitionConflict:
		return localDescriptor(code, "ElementDefinitionConflict",
							   "The Flow element definition conflicts with an existing definition.",
							   ErrorBoundary::ManagerOperation);
	case ErrorCode::ElementStateUnavailable:
		return localDescriptor(code, "ElementStateUnavailable",
							   "The Flow element state is unavailable.");
	case ErrorCode::ElementResourceUnavailable:
		return localDescriptor(code, "ElementResourceUnavailable",
							   "The Flow element resource is unavailable.");
	case ErrorCode::ElementResourceConstructionFailed:
		return {code,
				ErrorDomain::Element,
				ErrorCategory::Unwind,
				ErrorBoundary::WindowFrame,
				ErrorResolvability::CallerResolvable,
				ErrorImpact::FrameCanceled,
				ErrorResolutionPolicy::CallerDefined,
				ErrorResolution::Propagated,
				"ElementResourceConstructionFailed",
				"Flow element resource construction failed."};
	case ErrorCode::ElementResourceRecursiveConstruction:
		return localDescriptor(code, "ElementResourceRecursiveConstruction",
							   "Flow element resource construction is recursive.");
	case ErrorCode::ElementResourceDestroying:
		return localDescriptor(code, "ElementResourceDestroying",
							   "Flow element resources are being destroyed.");
	case ErrorCode::ElementStorageStale:
		return {code,
				ErrorDomain::Element,
				ErrorCategory::Fatal,
				ErrorBoundary::Process,
				ErrorResolvability::NotResolvable,
				ErrorImpact::Fatal,
				ErrorResolutionPolicy::None,
				ErrorResolution::Terminated,
				"ElementStorageStale",
				"Committed Flow element storage became stale."};

	case ErrorCode::InvalidActionId:
		return localDescriptor(code, "InvalidActionId", "The action identity is invalid.");
	case ErrorCode::ActionAlreadyBound:
		return localDescriptor(code, "ActionAlreadyBound", "The action is already bound.");
	case ErrorCode::ActionNotBound:
		return localDescriptor(code, "ActionNotBound", "The action is not bound.");
	case ErrorCode::ActionDisabled:
		return localDescriptor(code, "ActionDisabled", "The action is disabled.");
	case ErrorCode::ActionResultTypeMismatch:
		return localDescriptor(code, "ActionResultTypeMismatch",
							   "The action result type does not match the invocation.");
	case ErrorCode::ActionPublicationConflict:
		return {code,
				ErrorDomain::Action,
				ErrorCategory::Fatal,
				ErrorBoundary::Process,
				ErrorResolvability::NotResolvable,
				ErrorImpact::Fatal,
				ErrorResolutionPolicy::None,
				ErrorResolution::Terminated,
				"ActionPublicationConflict",
				"Action binding state changed during publication."};

	case ErrorCode::ThemeTypeNotFound:
		return localDescriptor(code, "ThemeTypeNotFound",
							   "The requested theme type is not registered.");
	case ErrorCode::ThemeVariantNotFound:
		return localDescriptor(code, "ThemeVariantNotFound",
							   "The requested theme variant is not registered.");
	case ErrorCode::ThemeActiveVariantMissing:
		return localDescriptor(code, "ThemeActiveVariantMissing",
							   "The theme type has no active variant.");
	case ErrorCode::ThemeMutationFailed:
		return {code,
				ErrorDomain::Theme,
				ErrorCategory::Unwind,
				ErrorBoundary::AppPoll,
				ErrorResolvability::CallerResolvable,
				ErrorImpact::OperationFailed,
				ErrorResolutionPolicy::CallerDefined,
				ErrorResolution::Propagated,
				"ThemeMutationFailed",
				"A staged theme mutation failed."};

	case ErrorCode::InputFieldUnavailable:
		return localDescriptor(code, "InputFieldUnavailable",
							   "The requested input field is unavailable.");
	case ErrorCode::InputRangeInvalid:
		return localDescriptor(code, "InputRangeInvalid", "The input edit range is invalid.");
	case ErrorCode::InputUtf8Invalid:
		return localDescriptor(code, "InputUtf8Invalid", "The input text is not valid UTF-8.");
	case ErrorCode::InputSizeLimitExceeded:
		return localDescriptor(code, "InputSizeLimitExceeded",
							   "The input text exceeds its configured size limit.");

	case ErrorCode::PopupFrameInactive:
		return localDescriptor(code, "PopupFrameInactive",
							   "The popup operation requires an active UI frame.");
	case ErrorCode::PopupDuplicateSubmission:
		return {code,
				ErrorDomain::Popup,
				ErrorCategory::Local,
				ErrorBoundary::ManagerOperation,
				ErrorResolvability::CallerResolvable,
				ErrorImpact::Degraded,
				ErrorResolutionPolicy::AppConfigured,
				ErrorResolution::Rejected,
				"PopupDuplicateSubmission",
				"The popup was submitted more than once in one frame."};
	case ErrorCode::PopupAnchorNotFound:
		return {code,
				ErrorDomain::Popup,
				ErrorCategory::Local,
				ErrorBoundary::ManagerOperation,
				ErrorResolvability::AutomaticallyResolved,
				ErrorImpact::Degraded,
				ErrorResolutionPolicy::AppConfigured,
				ErrorResolution::Skipped,
				"PopupAnchorNotFound",
				"The popup anchor was not found."};
	case ErrorCode::PopupCapacityExceeded:
		return {code,
				ErrorDomain::Popup,
				ErrorCategory::Local,
				ErrorBoundary::ManagerOperation,
				ErrorResolvability::AutomaticallyResolved,
				ErrorImpact::Degraded,
				ErrorResolutionPolicy::AppConfigured,
				ErrorResolution::Skipped,
				"PopupCapacityExceeded",
				"The popup capacity was exceeded."};

	case ErrorCode::ShortcutInvalid:
		return localDescriptor(code, "ShortcutInvalid", "The shortcut registration is invalid.");
	case ErrorCode::ShortcutIdSpaceExhausted:
		return localDescriptor(code, "ShortcutIdSpaceExhausted",
							   "The shortcut identity space was exhausted.",
							   ErrorBoundary::ManagerOperation);
	case ErrorCode::ShortcutNotFound:
		return localDescriptor(code, "ShortcutNotFound",
							   "The shortcut registration was not found.");

	case ErrorCode::ViewportDetached:
		return localDescriptor(code, "ViewportDetached", "The viewport is detached from storage.");
	case ErrorCode::ViewportConfigurationInvalid:
		return localDescriptor(code, "ViewportConfigurationInvalid",
							   "The viewport configuration is invalid.");
	case ErrorCode::ViewportNotFound:
		return {code,
				ErrorDomain::Viewport,
				ErrorCategory::Local,
				ErrorBoundary::ManagerOperation,
				ErrorResolvability::AutomaticallyResolved,
				ErrorImpact::Degraded,
				ErrorResolutionPolicy::AppConfigured,
				ErrorResolution::UsedFallback,
				"ViewportNotFound",
				"The requested viewport was not found."};
	case ErrorCode::ViewportKeyCollision:
		return localDescriptor(code, "ViewportKeyCollision",
							   "The viewport key is already registered.",
							   ErrorBoundary::ManagerOperation);
	case ErrorCode::ViewportGenerationIncomplete:
	case ErrorCode::ViewportGenerationExhausted:
		return {code,
				ErrorDomain::Viewport,
				ErrorCategory::Fatal,
				ErrorBoundary::Process,
				ErrorResolvability::NotResolvable,
				ErrorImpact::Fatal,
				ErrorResolutionPolicy::None,
				ErrorResolution::Terminated,
				code == ErrorCode::ViewportGenerationIncomplete ? "ViewportGenerationIncomplete"
																: "ViewportGenerationExhausted",
				code == ErrorCode::ViewportGenerationIncomplete
					? "A published viewport generation is incomplete."
					: "The viewport generation identity space was exhausted."};
	case ErrorCode::AgenticInvalidRequest:
		return localDescriptor(code, "AgenticInvalidRequest", "The agentic request is invalid.");
	case ErrorCode::AgenticUnavailable:
		return localDescriptor(code, "AgenticUnavailable",
							   "The requested agentic capability or target is unavailable.");
	case ErrorCode::AgenticBusy:
		return localDescriptor(code, "AgenticBusy",
							   "The application's capture lease or command queue is busy.");
	case ErrorCode::AgenticStaleGeneration:
		return localDescriptor(code, "AgenticStaleGeneration",
							   "The requested evidence or tooling revision changed.");
	case ErrorCode::AgenticInvalidScope:
		return localDescriptor(code, "AgenticInvalidScope",
							   "The request escapes its declared evidence scope.");
	case ErrorCode::AgenticBudgetExhausted:
		return localDescriptor(code, "AgenticBudgetExhausted",
							   "The agentic operation exceeded its configured budget.");
	case ErrorCode::AgenticIncompleteCapture:
		return localDescriptor(code, "AgenticIncompleteCapture",
							   "Required evidence has incomplete coverage.");
	case ErrorCode::AgenticPublicationFailed:
		return localDescriptor(code, "AgenticPublicationFailed",
							   "The evidence publication failed.");
	case ErrorCode::AgenticIncompatibleComparison:
		return localDescriptor(code, "AgenticIncompatibleComparison",
							   "The evidence is not comparable under the requested policy.");
	case ErrorCode::AgenticCanceled:
		return localDescriptor(code, "AgenticCanceled", "The agentic operation was canceled.");
	case ErrorCode::AgenticTimeout:
		return localDescriptor(code, "AgenticTimeout",
							   "The agentic operation reached its deadline.");
	case ErrorCode::AgenticIntegrityFailed:
		return localDescriptor(code, "AgenticIntegrityFailed",
							   "Evidence integrity or reference validation failed.");
	case ErrorCode::ViewportRecordingFailed:
		return {code,
				ErrorDomain::Viewport,
				ErrorCategory::Unwind,
				ErrorBoundary::WindowFrame,
				ErrorResolvability::CallerResolvable,
				ErrorImpact::FrameCanceled,
				ErrorResolutionPolicy::CallerDefined,
				ErrorResolution::Propagated,
				"ViewportRecordingFailed",
				"Viewport command recording failed."};
	}
	return {code,
			detail::errorDomainFromValue(code),
			ErrorCategory::Fatal,
			ErrorBoundary::Process,
			ErrorResolvability::NotResolvable,
			ErrorImpact::Fatal,
			ErrorResolutionPolicy::None,
			ErrorResolution::Terminated,
			"UnknownErrorCode",
			"An unknown FlowUi error code was observed."};
}

/** Return the static code name, or UnknownErrorCode for an unrecognized value. */
[[nodiscard]] constexpr std::string_view errorName(ErrorCode code) noexcept {
	return describeError(code).name;
}

/**
 * Return a static, null-terminated diagnostic message for code; unknown values receive the catalog
 * fallback message.
 */
[[nodiscard]] constexpr std::string_view errorMessage(ErrorCode code) noexcept {
	return describeError(code).message;
}

} // namespace FlowUi
