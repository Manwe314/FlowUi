#pragma once

#include <cstdio>
#include <cstdint>
#include <exception>
#include <expected>
#include <string_view>
#include <type_traits>
#include <utility>
#include <variant>

#include "FlowUi/BuildConfig.hpp"

namespace FlowUi {

/**
 * How a deliberately defined FlowUi failure travels to its resolution boundary.
 * This is independent from ErrorImpact: transport distance is not severity.
 */
enum class ErrorCategory : std::uint8_t {
	Static = 0,
	Local,
	Unwind,
	Fatal,
};

/** Stable ownership domain for an error code. */
enum class ErrorDomain : std::uint8_t {
	None = 0,
	Core,
	App,
	Platform,
	Window,
	Frame,
	Vulkan,
	Renderer,
	Storage,
	Resource,
	Asset,
	Font,
	Image,
	Icon,
	Element,
	Action,
	Theme,
	Input,
	Popup,
	Shortcut,
	Viewport,
};

/** The boundary that is intended to decide the outcome of an error. */
enum class ErrorBoundary : std::uint8_t {
	CompileExpression = 0,
	DirectCaller,
	ManagerOperation,
	WindowFrame,
	WindowLifecycle,
	AppPoll,
	AppLifecycle,
	Process,
};

/** Whether valid continuation exists, and who can select it. */
enum class ErrorResolvability : std::uint8_t {
	AutomaticallyResolved = 0,
	CallerResolvable,
	RecreateWindow,
	RecreateApp,
	NotResolvable,
};

/** Consequence of an occurrence after its configured/default resolution. */
enum class ErrorImpact : std::uint8_t {
	None = 0,
	Degraded,
	OperationFailed,
	FrameCanceled,
	WindowUnavailable,
	AppUnavailable,
	Fatal,
};

/** Who is allowed to select a resolution for this code. */
enum class ErrorResolutionPolicy : std::uint8_t {
	HardCoded = 0,
	AppConfigured,
	CallerDefined,
	None,
};

/** Stable resolution vocabulary shared by observers and development reports. */
enum class ErrorResolution : std::uint8_t {
	None = 0,
	Rejected,
	Ignored,
	Retried,
	UsedFallback,
	Skipped,
	GrewCapacity,
	EvictedAndRetried,
	CanceledOperation,
	CanceledFrame,
	RecreatedWindow,
	ClosedWindow,
	RecreatedApp,
	Propagated,
	Halted,
	Terminated,
};

/** App-creation policies for deliberately recoverable error families. */
enum class DefaultFontFailurePolicy : std::uint8_t {
	TryFallbackThenDisableText = 0,
	TryFallbackThenFailApp,
	FailAppImmediately,
};

enum class MissingVisualPolicy : std::uint8_t {
	UseFallbackTexture = 0,
	SkipVisual,
};

enum class IconGenerationFailurePolicy : std::uint8_t {
	UseFallbackTexture = 0,
	SkipVisual,
	FailRequest,
};

enum class StorageCapacityPolicy : std::uint8_t {
	GrowWithinBudget = 0,
	RejectOperation,
};

enum class CacheCapacityPolicy : std::uint8_t {
	EvictAndRetry = 0,
	RejectOperation,
};

enum class PopupDuplicatePolicy : std::uint8_t {
	FirstSubmissionWins = 0,
	RejectDuplicate,
};

enum class PopupMissingAnchorPolicy : std::uint8_t {
	SkipPopup = 0,
	DeferUntilAnchorExists,
	AnchorToViewport,
};

enum class PopupCapacityPolicy : std::uint8_t {
	ClampLayer = 0,
	SkipOverflowingPopup,
};

enum class InputQueueOverflowPolicy : std::uint8_t {
	DropNewest = 0,
	DropOldest,
};

/**
 * Error resolutions selected once at App creation. Development mode may observe
 * these decisions but never changes them.
 */
struct ErrorPolicy {
	DefaultFontFailurePolicy defaultFont = DefaultFontFailurePolicy::TryFallbackThenDisableText;
	MissingVisualPolicy missingImage = MissingVisualPolicy::UseFallbackTexture;
	MissingVisualPolicy missingViewport = MissingVisualPolicy::UseFallbackTexture;
	IconGenerationFailurePolicy iconGeneration = IconGenerationFailurePolicy::UseFallbackTexture;
	StorageCapacityPolicy transientCapacity = StorageCapacityPolicy::GrowWithinBudget;
	StorageCapacityPolicy persistentCapacity = StorageCapacityPolicy::GrowWithinBudget;
	CacheCapacityPolicy descriptorCapacity = CacheCapacityPolicy::RejectOperation;
	PopupDuplicatePolicy duplicatePopup = PopupDuplicatePolicy::FirstSubmissionWins;
	PopupMissingAnchorPolicy missingPopupAnchor = PopupMissingAnchorPolicy::SkipPopup;
	PopupCapacityPolicy popupCapacity = PopupCapacityPolicy::ClampLayer;
	InputQueueOverflowPolicy inputQueueOverflow = InputQueueOverflowPolicy::DropNewest;
	std::uint32_t inputTextQueueCapacity = 4096;
};

/** Kind of stable numeric subject carried by FlowUiError::subject. */
enum class ErrorSubjectKind : std::uint8_t {
	None = 0,
	App,
	Window,
	Frame,
	Submission,
	Resource,
	ResourceKey,
	ElementDefinition,
	ElementInstance,
	Action,
	Theme,
	Font,
	Image,
	Icon,
	Popup,
	Shortcut,
	Viewport,
	MemoryTarget,
};

/**
 * Stable identity of the FlowUi operation that detected an error.
 *
 * A site is an operation-level production diagnostic, not a source location.
 * Values are explicitly grouped and numbered because they are part of the
 * public error contract. Published values must never be reused.
 */
enum class ErrorSite : std::uint16_t {
	None = 0x0000,

	AppConstruct = 0x0101,
	AppInitialize = 0x0102,
	AppPollEvents = 0x0103,
	AppCreateWindow = 0x0104,
	AppDestroyWindow = 0x0105,
	AppBeginFrame = 0x0106,
	AppEndFrame = 0x0107,
	AppDrawFrame = 0x0108,
	AppRecreateWindow = 0x0109,
	AppAcquireFrame = 0x010a,
	AppSubmitFrame = 0x010b,
	AppPresent = 0x010c,
	AppShutdown = 0x010d,
	AppAccessFonts = 0x010e,
	AppAccessImages = 0x010f,
	AppAccessThemes = 0x0110,
	AppAccessElements = 0x0111,
	AppAccessActions = 0x0112,
	AppAccessIcons = 0x0113,
	AppAccessViewports = 0x0114,
	AppAccessUi = 0x0115,
	AppAccessWindow = 0x0116,
	AppAccessDevMonitoring = 0x0117,
	AppRequireQuiescent = 0x0118,
	AppRequirePlatformThread = 0x0119,
	AppFrameDispatch = 0x011a,

	WindowBackendInitialize = 0x0201,
	WindowMonitorSelect = 0x0202,
	WindowCreate = 0x0203,
	WindowDestroy = 0x0204,
	WindowRequiredExtensions = 0x0205,
	WindowSurfaceCreate = 0x0206,
	WindowPollInput = 0x0207,
	WindowSetTitle = 0x0208,
	WindowSetCursor = 0x0209,
	WindowReadClipboard = 0x020a,
	WindowWriteClipboard = 0x020b,
	WindowBackendDiagnostic = 0x020c,

	VulkanContextInitialize = 0x0301,
	VulkanInstanceCreate = 0x0302,
	VulkanSurfaceQuery = 0x0303,
	VulkanPhysicalDeviceSelect = 0x0304,
	VulkanLogicalDeviceCreate = 0x0305,
	VulkanAllocatorCreate = 0x0306,
	VulkanFramesInitialize = 0x0307,
	VulkanFrameAcquire = 0x0308,
	VulkanFrameSubmit = 0x0309,
	VulkanSwapchainCreate = 0x030a,
	VulkanSwapchainAcquire = 0x030b,
	VulkanSwapchainPresent = 0x030c,
	VulkanSwapchainRecreate = 0x030d,
	VulkanContextShutdown = 0x030e,
	VulkanDebugDiagnostic = 0x030f,

	RendererInitialize = 0x0401,
	RendererLoadShader = 0x0402,
	RendererPublishLayout = 0x0403,
	RendererPublishPipeline = 0x0404,
	RendererCreateWindowResources = 0x0405,
	RendererConvertCommands = 0x0406,
	RendererRecordCommands = 0x0407,
	RendererPublishDescriptors = 0x0408,
	RendererDraw = 0x0409,
	RendererDestroyWindowResources = 0x040a,
	RendererShutdown = 0x040b,

	StorageInitialize = 0x0501,
	StorageRegisterWindow = 0x0502,
	StorageUnregisterWindow = 0x0503,
	StorageBeginFrame = 0x0504,
	StorageSealFrame = 0x0505,
	StorageCancelFrame = 0x0506,
	StorageValidateFrame = 0x0507,
	StorageValidateSubmission = 0x0508,
	StorageNoteSubmission = 0x0509,
	StorageNoteCompletion = 0x050a,
	StorageRetire = 0x050b,
	StorageCollect = 0x050c,
	StorageTrim = 0x050d,
	StorageSetBudget = 0x050e,
	StorageShutdown = 0x050f,
	StorageCalculateCapacity = 0x0510,
	StorageRequireInitialized = 0x0511,
	StorageLookupWindow = 0x0512,

	ResourceNormalizeKey = 0x0601,
	ResourceInternKey = 0x0602,
	ResourceAllocatePersistent = 0x0603,
	ResourceCreateManagerRecord = 0x0604,
	ResourceLookupManagerRecord = 0x0605,
	ResourceRemoveManagerRecord = 0x0606,
	ResourceCreatePersistentRecord = 0x0607,
	ResourceLookupPersistentRecord = 0x0608,
	ResourceRemovePersistentRecord = 0x0609,
	ResourceCreateBlob = 0x060a,
	ResourceReadBlob = 0x060b,
	ResourceCreateBuffer = 0x060c,
	ResourceWriteBuffer = 0x060d,
	ResourceCreateImage = 0x060e,
	ResourceCreateImageView = 0x060f,
	ResourceAcquireSampler = 0x0610,
	ResourcePublishTexture = 0x0611,
	ResourceReplaceTexture = 0x0612,
	ResourceLookupTexture = 0x0613,
	ResourceRemoveTexture = 0x0614,
	ResourcePrepareTextureBindings = 0x0615,
	ResourcePublishTextureBindings = 0x0616,
	ResourceResolveTexture = 0x0617,
	ResourceEnqueueUpload = 0x0618,
	ResourceFlushUploads = 0x0619,
	ResourceValidateHandle = 0x061a,
	ResourcePublishNativeObject = 0x061b,
	ResourceAcquireNativeObject = 0x061c,

	UiManagerInitialize = 0x0701,
	UiManagerBeginFrame = 0x0702,
	UiManagerEndFrame = 0x0703,
	UiManagerRender = 0x0704,
	UiManagerReset = 0x0705,
	UiManagerDefineElement = 0x0706,
	UiManagerOpenElement = 0x0707,
	UiManagerCloseElement = 0x0708,
	UiManagerInvokeElement = 0x0709,
	UiManagerShutdown = 0x070a,
	UiManagerAccessElements = 0x070b,
	UiManagerAccessActions = 0x070c,
	UiManagerAccessTheme = 0x070d,

	ElementManagerInitialize = 0x0801,
	ElementRegisterDefinition = 0x0802,
	ElementRegisterWindow = 0x0803,
	ElementBeginFrame = 0x0804,
	ElementResolveState = 0x0805,
	ElementConstructState = 0x0806,
	ElementEraseState = 0x0807,
	ElementResolveResource = 0x0808,
	ElementConstructResource = 0x0809,
	ElementDestroyResource = 0x080a,
	ElementCommitFrame = 0x080b,
	ElementCancelFrame = 0x080c,
	ElementInvoke = 0x080d,
	ElementManagerShutdown = 0x080e,

	ActionManagerInitialize = 0x0901,
	ActionBind = 0x0902,
	ActionRebind = 0x0903,
	ActionUnbind = 0x0904,
	ActionSetAvailability = 0x0905,
	ActionPublish = 0x0906,
	ActionInvoke = 0x0907,
	ActionCompleteInvocation = 0x0908,
	ActionManagerShutdown = 0x0909,

	ThemeManagerInitialize = 0x0a01,
	ThemeRegisterType = 0x0a02,
	ThemeRegisterVariant = 0x0a03,
	ThemeSetActiveVariant = 0x0a04,
	ThemeLookupActiveVariant = 0x0a05,
	ThemeMutate = 0x0a06,
	ThemeStageMutation = 0x0a07,
	ThemeCommitMutation = 0x0a08,
	ThemeRollbackMutation = 0x0a09,
	ThemeManagerShutdown = 0x0a0a,

	InputManagerInitialize = 0x0b01,
	InputRegisterField = 0x0b02,
	InputBeginFrame = 0x0b03,
	InputEdit = 0x0b04,
	InputCopy = 0x0b05,
	InputCut = 0x0b06,
	InputPaste = 0x0b07,
	InputFocus = 0x0b08,
	InputBlur = 0x0b09,
	InputSubmit = 0x0b0a,
	InputQueueEvent = 0x0b0b,
	InputManagerShutdown = 0x0b0c,

	PopupManagerInitialize = 0x0c01,
	PopupBeginFrame = 0x0c02,
	PopupRequest = 0x0c03,
	PopupResolveAnchor = 0x0c04,
	PopupMeasure = 0x0c05,
	PopupLayout = 0x0c06,
	PopupDismiss = 0x0c07,
	PopupEndFrame = 0x0c08,
	PopupManagerShutdown = 0x0c09,

	ShortcutManagerInitialize = 0x0d01,
	ShortcutRegister = 0x0d02,
	ShortcutUpdate = 0x0d03,
	ShortcutRemove = 0x0d04,
	ShortcutResolve = 0x0d05,
	ShortcutManagerShutdown = 0x0d06,

	AssetOpen = 0x0e01,
	AssetRead = 0x0e02,
	AssetValidatePayload = 0x0e03,
	AssetDecode = 0x0e04,

	ImageManagerInitialize = 0x0f01,
	ImageRegister = 0x0f02,
	ImageLoad = 0x0f03,
	ImageDecode = 0x0f04,
	ImageUpload = 0x0f05,
	ImagePublish = 0x0f06,
	ImageLookup = 0x0f07,
	ImageRemove = 0x0f08,
	ImageManagerShutdown = 0x0f09,

	FontManagerInitialize = 0x1001,
	FontRegisterFamily = 0x1002,
	FontLoad = 0x1003,
	FontParse = 0x1004,
	FontBake = 0x1005,
	FontPublishAtlas = 0x1006,
	FontLookupFamily = 0x1007,
	FontRemoveFamily = 0x1008,
	FontManagerShutdown = 0x1009,

	IconManagerInitialize = 0x1101,
	IconRegisterSource = 0x1102,
	IconParseSource = 0x1103,
	IconRegisterRaster = 0x1104,
	IconRasterize = 0x1105,
	IconPublishAtlas = 0x1106,
	IconLookup = 0x1107,
	IconManagerShutdown = 0x1108,
	IconRemove = 0x1109,

	ViewportManagerInitialize = 0x1201,
	ViewportCreate = 0x1202,
	ViewportResize = 0x1203,
	ViewportPrepare = 0x1204,
	ViewportRecord = 0x1205,
	ViewportPublish = 0x1206,
	ViewportLookup = 0x1207,
	ViewportDestroy = 0x1208,
	ViewportManagerShutdown = 0x1209,
	ViewportConfigure = 0x120a,

	CallbackAction = 0x1301,
	CallbackElementConstruct = 0x1302,
	CallbackElementDestroy = 0x1303,
	CallbackElementInteraction = 0x1304,
	CallbackThemeMutation = 0x1305,
	CallbackWindow = 0x1306,
	CallbackViewportRender = 0x1307,
	CleanupFrameRollback = 0x1308,
	CleanupResourceRollback = 0x1309,
	CleanupManagerShutdown = 0x130a,
	CleanupAppShutdown = 0x130b,
};

/** Static catalog data associated with one ErrorSite. */
struct ErrorSiteDescriptor {
	ErrorSite site = ErrorSite::None;
	ErrorSubjectKind expectedSubject = ErrorSubjectKind::None;
	std::string_view name = "None";
};

/** Return the descriptor for a stable error site. */
[[nodiscard]] constexpr ErrorSiteDescriptor describeErrorSite(ErrorSite site) noexcept {
#define FLOWUI_ERROR_SITE_CASE(siteName, subjectKind) \
	case ErrorSite::siteName: return {ErrorSite::siteName, ErrorSubjectKind::subjectKind, #siteName}
	switch (site) {
	FLOWUI_ERROR_SITE_CASE(None, None);

	FLOWUI_ERROR_SITE_CASE(AppConstruct, App);
	FLOWUI_ERROR_SITE_CASE(AppInitialize, App);
	FLOWUI_ERROR_SITE_CASE(AppPollEvents, App);
	FLOWUI_ERROR_SITE_CASE(AppCreateWindow, Window);
	FLOWUI_ERROR_SITE_CASE(AppDestroyWindow, Window);
	FLOWUI_ERROR_SITE_CASE(AppBeginFrame, Window);
	FLOWUI_ERROR_SITE_CASE(AppEndFrame, Window);
	FLOWUI_ERROR_SITE_CASE(AppDrawFrame, Window);
	FLOWUI_ERROR_SITE_CASE(AppRecreateWindow, Window);
	FLOWUI_ERROR_SITE_CASE(AppAcquireFrame, Frame);
	FLOWUI_ERROR_SITE_CASE(AppSubmitFrame, Frame);
	FLOWUI_ERROR_SITE_CASE(AppPresent, Window);
	FLOWUI_ERROR_SITE_CASE(AppShutdown, App);
	FLOWUI_ERROR_SITE_CASE(AppAccessFonts, App);
	FLOWUI_ERROR_SITE_CASE(AppAccessImages, App);
	FLOWUI_ERROR_SITE_CASE(AppAccessThemes, App);
	FLOWUI_ERROR_SITE_CASE(AppAccessElements, App);
	FLOWUI_ERROR_SITE_CASE(AppAccessActions, App);
	FLOWUI_ERROR_SITE_CASE(AppAccessIcons, App);
	FLOWUI_ERROR_SITE_CASE(AppAccessViewports, Window);
	FLOWUI_ERROR_SITE_CASE(AppAccessUi, Window);
	FLOWUI_ERROR_SITE_CASE(AppAccessWindow, Window);
	FLOWUI_ERROR_SITE_CASE(AppAccessDevMonitoring, App);
	FLOWUI_ERROR_SITE_CASE(AppRequireQuiescent, Window);
	FLOWUI_ERROR_SITE_CASE(AppRequirePlatformThread, App);
	FLOWUI_ERROR_SITE_CASE(AppFrameDispatch, Window);

	FLOWUI_ERROR_SITE_CASE(WindowBackendInitialize, None);
	FLOWUI_ERROR_SITE_CASE(WindowMonitorSelect, None);
	FLOWUI_ERROR_SITE_CASE(WindowCreate, Window);
	FLOWUI_ERROR_SITE_CASE(WindowDestroy, Window);
	FLOWUI_ERROR_SITE_CASE(WindowRequiredExtensions, None);
	FLOWUI_ERROR_SITE_CASE(WindowSurfaceCreate, Window);
	FLOWUI_ERROR_SITE_CASE(WindowPollInput, Window);
	FLOWUI_ERROR_SITE_CASE(WindowSetTitle, Window);
	FLOWUI_ERROR_SITE_CASE(WindowSetCursor, Window);
	FLOWUI_ERROR_SITE_CASE(WindowReadClipboard, Window);
	FLOWUI_ERROR_SITE_CASE(WindowWriteClipboard, Window);
	FLOWUI_ERROR_SITE_CASE(WindowBackendDiagnostic, None);

	FLOWUI_ERROR_SITE_CASE(VulkanContextInitialize, App);
	FLOWUI_ERROR_SITE_CASE(VulkanInstanceCreate, App);
	FLOWUI_ERROR_SITE_CASE(VulkanSurfaceQuery, Window);
	FLOWUI_ERROR_SITE_CASE(VulkanPhysicalDeviceSelect, App);
	FLOWUI_ERROR_SITE_CASE(VulkanLogicalDeviceCreate, App);
	FLOWUI_ERROR_SITE_CASE(VulkanAllocatorCreate, App);
	FLOWUI_ERROR_SITE_CASE(VulkanFramesInitialize, Window);
	FLOWUI_ERROR_SITE_CASE(VulkanFrameAcquire, Frame);
	FLOWUI_ERROR_SITE_CASE(VulkanFrameSubmit, Frame);
	FLOWUI_ERROR_SITE_CASE(VulkanSwapchainCreate, Window);
	FLOWUI_ERROR_SITE_CASE(VulkanSwapchainAcquire, Window);
	FLOWUI_ERROR_SITE_CASE(VulkanSwapchainPresent, Window);
	FLOWUI_ERROR_SITE_CASE(VulkanSwapchainRecreate, Window);
	FLOWUI_ERROR_SITE_CASE(VulkanContextShutdown, App);
	FLOWUI_ERROR_SITE_CASE(VulkanDebugDiagnostic, App);

	FLOWUI_ERROR_SITE_CASE(RendererInitialize, None);
	FLOWUI_ERROR_SITE_CASE(RendererLoadShader, None);
	FLOWUI_ERROR_SITE_CASE(RendererPublishLayout, Resource);
	FLOWUI_ERROR_SITE_CASE(RendererPublishPipeline, Resource);
	FLOWUI_ERROR_SITE_CASE(RendererCreateWindowResources, Window);
	FLOWUI_ERROR_SITE_CASE(RendererConvertCommands, Frame);
	FLOWUI_ERROR_SITE_CASE(RendererRecordCommands, Frame);
	FLOWUI_ERROR_SITE_CASE(RendererPublishDescriptors, Window);
	FLOWUI_ERROR_SITE_CASE(RendererDraw, Frame);
	FLOWUI_ERROR_SITE_CASE(RendererDestroyWindowResources, Window);
	FLOWUI_ERROR_SITE_CASE(RendererShutdown, None);

	FLOWUI_ERROR_SITE_CASE(StorageInitialize, None);
	FLOWUI_ERROR_SITE_CASE(StorageRegisterWindow, Window);
	FLOWUI_ERROR_SITE_CASE(StorageUnregisterWindow, Window);
	FLOWUI_ERROR_SITE_CASE(StorageBeginFrame, Frame);
	FLOWUI_ERROR_SITE_CASE(StorageSealFrame, Frame);
	FLOWUI_ERROR_SITE_CASE(StorageCancelFrame, Frame);
	FLOWUI_ERROR_SITE_CASE(StorageValidateFrame, Frame);
	FLOWUI_ERROR_SITE_CASE(StorageValidateSubmission, Submission);
	FLOWUI_ERROR_SITE_CASE(StorageNoteSubmission, Submission);
	FLOWUI_ERROR_SITE_CASE(StorageNoteCompletion, Submission);
	FLOWUI_ERROR_SITE_CASE(StorageRetire, Resource);
	FLOWUI_ERROR_SITE_CASE(StorageCollect, Resource);
	FLOWUI_ERROR_SITE_CASE(StorageTrim, MemoryTarget);
	FLOWUI_ERROR_SITE_CASE(StorageSetBudget, MemoryTarget);
	FLOWUI_ERROR_SITE_CASE(StorageShutdown, None);
	FLOWUI_ERROR_SITE_CASE(StorageCalculateCapacity, MemoryTarget);
	FLOWUI_ERROR_SITE_CASE(StorageRequireInitialized, None);
	FLOWUI_ERROR_SITE_CASE(StorageLookupWindow, Window);

	FLOWUI_ERROR_SITE_CASE(ResourceNormalizeKey, ResourceKey);
	FLOWUI_ERROR_SITE_CASE(ResourceInternKey, ResourceKey);
	FLOWUI_ERROR_SITE_CASE(ResourceAllocatePersistent, MemoryTarget);
	FLOWUI_ERROR_SITE_CASE(ResourceCreateManagerRecord, ResourceKey);
	FLOWUI_ERROR_SITE_CASE(ResourceLookupManagerRecord, ResourceKey);
	FLOWUI_ERROR_SITE_CASE(ResourceRemoveManagerRecord, ResourceKey);
	FLOWUI_ERROR_SITE_CASE(ResourceCreatePersistentRecord, Resource);
	FLOWUI_ERROR_SITE_CASE(ResourceLookupPersistentRecord, Resource);
	FLOWUI_ERROR_SITE_CASE(ResourceRemovePersistentRecord, Resource);
	FLOWUI_ERROR_SITE_CASE(ResourceCreateBlob, Resource);
	FLOWUI_ERROR_SITE_CASE(ResourceReadBlob, Resource);
	FLOWUI_ERROR_SITE_CASE(ResourceCreateBuffer, Resource);
	FLOWUI_ERROR_SITE_CASE(ResourceWriteBuffer, Resource);
	FLOWUI_ERROR_SITE_CASE(ResourceCreateImage, Resource);
	FLOWUI_ERROR_SITE_CASE(ResourceCreateImageView, Resource);
	FLOWUI_ERROR_SITE_CASE(ResourceAcquireSampler, Resource);
	FLOWUI_ERROR_SITE_CASE(ResourcePublishTexture, ResourceKey);
	FLOWUI_ERROR_SITE_CASE(ResourceReplaceTexture, ResourceKey);
	FLOWUI_ERROR_SITE_CASE(ResourceLookupTexture, ResourceKey);
	FLOWUI_ERROR_SITE_CASE(ResourceRemoveTexture, ResourceKey);
	FLOWUI_ERROR_SITE_CASE(ResourcePrepareTextureBindings, Frame);
	FLOWUI_ERROR_SITE_CASE(ResourcePublishTextureBindings, Frame);
	FLOWUI_ERROR_SITE_CASE(ResourceResolveTexture, Resource);
	FLOWUI_ERROR_SITE_CASE(ResourceEnqueueUpload, Resource);
	FLOWUI_ERROR_SITE_CASE(ResourceFlushUploads, Resource);
	FLOWUI_ERROR_SITE_CASE(ResourceValidateHandle, Resource);
	FLOWUI_ERROR_SITE_CASE(ResourcePublishNativeObject, Resource);
	FLOWUI_ERROR_SITE_CASE(ResourceAcquireNativeObject, Resource);

	FLOWUI_ERROR_SITE_CASE(UiManagerInitialize, None);
	FLOWUI_ERROR_SITE_CASE(UiManagerBeginFrame, Window);
	FLOWUI_ERROR_SITE_CASE(UiManagerEndFrame, Window);
	FLOWUI_ERROR_SITE_CASE(UiManagerRender, Window);
	FLOWUI_ERROR_SITE_CASE(UiManagerReset, Window);
	FLOWUI_ERROR_SITE_CASE(UiManagerDefineElement, ElementDefinition);
	FLOWUI_ERROR_SITE_CASE(UiManagerOpenElement, ElementInstance);
	FLOWUI_ERROR_SITE_CASE(UiManagerCloseElement, ElementInstance);
	FLOWUI_ERROR_SITE_CASE(UiManagerInvokeElement, ElementInstance);
	FLOWUI_ERROR_SITE_CASE(UiManagerShutdown, None);
	FLOWUI_ERROR_SITE_CASE(UiManagerAccessElements, Window);
	FLOWUI_ERROR_SITE_CASE(UiManagerAccessActions, Window);
	FLOWUI_ERROR_SITE_CASE(UiManagerAccessTheme, Theme);

	FLOWUI_ERROR_SITE_CASE(ElementManagerInitialize, None);
	FLOWUI_ERROR_SITE_CASE(ElementRegisterDefinition, ElementDefinition);
	FLOWUI_ERROR_SITE_CASE(ElementRegisterWindow, Window);
	FLOWUI_ERROR_SITE_CASE(ElementBeginFrame, Window);
	FLOWUI_ERROR_SITE_CASE(ElementResolveState, ElementInstance);
	FLOWUI_ERROR_SITE_CASE(ElementConstructState, ElementInstance);
	FLOWUI_ERROR_SITE_CASE(ElementEraseState, ElementInstance);
	FLOWUI_ERROR_SITE_CASE(ElementResolveResource, ElementInstance);
	FLOWUI_ERROR_SITE_CASE(ElementConstructResource, ElementInstance);
	FLOWUI_ERROR_SITE_CASE(ElementDestroyResource, ElementInstance);
	FLOWUI_ERROR_SITE_CASE(ElementCommitFrame, Window);
	FLOWUI_ERROR_SITE_CASE(ElementCancelFrame, Window);
	FLOWUI_ERROR_SITE_CASE(ElementInvoke, ElementInstance);
	FLOWUI_ERROR_SITE_CASE(ElementManagerShutdown, None);

	FLOWUI_ERROR_SITE_CASE(ActionManagerInitialize, None);
	FLOWUI_ERROR_SITE_CASE(ActionBind, Action);
	FLOWUI_ERROR_SITE_CASE(ActionRebind, Action);
	FLOWUI_ERROR_SITE_CASE(ActionUnbind, Action);
	FLOWUI_ERROR_SITE_CASE(ActionSetAvailability, Action);
	FLOWUI_ERROR_SITE_CASE(ActionPublish, Action);
	FLOWUI_ERROR_SITE_CASE(ActionInvoke, Action);
	FLOWUI_ERROR_SITE_CASE(ActionCompleteInvocation, Action);
	FLOWUI_ERROR_SITE_CASE(ActionManagerShutdown, None);

	FLOWUI_ERROR_SITE_CASE(ThemeManagerInitialize, None);
	FLOWUI_ERROR_SITE_CASE(ThemeRegisterType, Theme);
	FLOWUI_ERROR_SITE_CASE(ThemeRegisterVariant, Theme);
	FLOWUI_ERROR_SITE_CASE(ThemeSetActiveVariant, Theme);
	FLOWUI_ERROR_SITE_CASE(ThemeLookupActiveVariant, Theme);
	FLOWUI_ERROR_SITE_CASE(ThemeMutate, Theme);
	FLOWUI_ERROR_SITE_CASE(ThemeStageMutation, Theme);
	FLOWUI_ERROR_SITE_CASE(ThemeCommitMutation, Theme);
	FLOWUI_ERROR_SITE_CASE(ThemeRollbackMutation, Theme);
	FLOWUI_ERROR_SITE_CASE(ThemeManagerShutdown, None);

	FLOWUI_ERROR_SITE_CASE(InputManagerInitialize, None);
	FLOWUI_ERROR_SITE_CASE(InputRegisterField, ElementInstance);
	FLOWUI_ERROR_SITE_CASE(InputBeginFrame, Window);
	FLOWUI_ERROR_SITE_CASE(InputEdit, ElementInstance);
	FLOWUI_ERROR_SITE_CASE(InputCopy, ElementInstance);
	FLOWUI_ERROR_SITE_CASE(InputCut, ElementInstance);
	FLOWUI_ERROR_SITE_CASE(InputPaste, ElementInstance);
	FLOWUI_ERROR_SITE_CASE(InputFocus, ElementInstance);
	FLOWUI_ERROR_SITE_CASE(InputBlur, ElementInstance);
	FLOWUI_ERROR_SITE_CASE(InputSubmit, ElementInstance);
	FLOWUI_ERROR_SITE_CASE(InputQueueEvent, Window);
	FLOWUI_ERROR_SITE_CASE(InputManagerShutdown, None);

	FLOWUI_ERROR_SITE_CASE(PopupManagerInitialize, None);
	FLOWUI_ERROR_SITE_CASE(PopupBeginFrame, Window);
	FLOWUI_ERROR_SITE_CASE(PopupRequest, Popup);
	FLOWUI_ERROR_SITE_CASE(PopupResolveAnchor, Popup);
	FLOWUI_ERROR_SITE_CASE(PopupMeasure, Popup);
	FLOWUI_ERROR_SITE_CASE(PopupLayout, Popup);
	FLOWUI_ERROR_SITE_CASE(PopupDismiss, Popup);
	FLOWUI_ERROR_SITE_CASE(PopupEndFrame, Window);
	FLOWUI_ERROR_SITE_CASE(PopupManagerShutdown, None);

	FLOWUI_ERROR_SITE_CASE(ShortcutManagerInitialize, None);
	FLOWUI_ERROR_SITE_CASE(ShortcutRegister, Shortcut);
	FLOWUI_ERROR_SITE_CASE(ShortcutUpdate, Shortcut);
	FLOWUI_ERROR_SITE_CASE(ShortcutRemove, Shortcut);
	FLOWUI_ERROR_SITE_CASE(ShortcutResolve, Shortcut);
	FLOWUI_ERROR_SITE_CASE(ShortcutManagerShutdown, None);

	FLOWUI_ERROR_SITE_CASE(AssetOpen, None);
	FLOWUI_ERROR_SITE_CASE(AssetRead, None);
	FLOWUI_ERROR_SITE_CASE(AssetValidatePayload, None);
	FLOWUI_ERROR_SITE_CASE(AssetDecode, None);

	FLOWUI_ERROR_SITE_CASE(ImageManagerInitialize, None);
	FLOWUI_ERROR_SITE_CASE(ImageRegister, Image);
	FLOWUI_ERROR_SITE_CASE(ImageLoad, Image);
	FLOWUI_ERROR_SITE_CASE(ImageDecode, Image);
	FLOWUI_ERROR_SITE_CASE(ImageUpload, Image);
	FLOWUI_ERROR_SITE_CASE(ImagePublish, Image);
	FLOWUI_ERROR_SITE_CASE(ImageLookup, Image);
	FLOWUI_ERROR_SITE_CASE(ImageRemove, Image);
	FLOWUI_ERROR_SITE_CASE(ImageManagerShutdown, None);

	FLOWUI_ERROR_SITE_CASE(FontManagerInitialize, None);
	FLOWUI_ERROR_SITE_CASE(FontRegisterFamily, Font);
	FLOWUI_ERROR_SITE_CASE(FontLoad, Font);
	FLOWUI_ERROR_SITE_CASE(FontParse, Font);
	FLOWUI_ERROR_SITE_CASE(FontBake, Font);
	FLOWUI_ERROR_SITE_CASE(FontPublishAtlas, Font);
	FLOWUI_ERROR_SITE_CASE(FontLookupFamily, Font);
	FLOWUI_ERROR_SITE_CASE(FontRemoveFamily, Font);
	FLOWUI_ERROR_SITE_CASE(FontManagerShutdown, None);

	FLOWUI_ERROR_SITE_CASE(IconManagerInitialize, None);
	FLOWUI_ERROR_SITE_CASE(IconRegisterSource, Icon);
	FLOWUI_ERROR_SITE_CASE(IconParseSource, Icon);
	FLOWUI_ERROR_SITE_CASE(IconRegisterRaster, Icon);
	FLOWUI_ERROR_SITE_CASE(IconRasterize, Icon);
	FLOWUI_ERROR_SITE_CASE(IconPublishAtlas, Icon);
	FLOWUI_ERROR_SITE_CASE(IconLookup, Icon);
	FLOWUI_ERROR_SITE_CASE(IconManagerShutdown, None);
	FLOWUI_ERROR_SITE_CASE(IconRemove, Icon);

	FLOWUI_ERROR_SITE_CASE(ViewportManagerInitialize, None);
	FLOWUI_ERROR_SITE_CASE(ViewportCreate, Viewport);
	FLOWUI_ERROR_SITE_CASE(ViewportResize, Viewport);
	FLOWUI_ERROR_SITE_CASE(ViewportPrepare, Viewport);
	FLOWUI_ERROR_SITE_CASE(ViewportRecord, Viewport);
	FLOWUI_ERROR_SITE_CASE(ViewportPublish, Viewport);
	FLOWUI_ERROR_SITE_CASE(ViewportLookup, Viewport);
	FLOWUI_ERROR_SITE_CASE(ViewportDestroy, Viewport);
	FLOWUI_ERROR_SITE_CASE(ViewportManagerShutdown, None);
	FLOWUI_ERROR_SITE_CASE(ViewportConfigure, Viewport);

	FLOWUI_ERROR_SITE_CASE(CallbackAction, Action);
	FLOWUI_ERROR_SITE_CASE(CallbackElementConstruct, ElementInstance);
	FLOWUI_ERROR_SITE_CASE(CallbackElementDestroy, ElementInstance);
	FLOWUI_ERROR_SITE_CASE(CallbackElementInteraction, ElementInstance);
	FLOWUI_ERROR_SITE_CASE(CallbackThemeMutation, Theme);
	FLOWUI_ERROR_SITE_CASE(CallbackWindow, Window);
	FLOWUI_ERROR_SITE_CASE(CallbackViewportRender, Viewport);
	FLOWUI_ERROR_SITE_CASE(CleanupFrameRollback, Frame);
	FLOWUI_ERROR_SITE_CASE(CleanupResourceRollback, Resource);
	FLOWUI_ERROR_SITE_CASE(CleanupManagerShutdown, None);
	FLOWUI_ERROR_SITE_CASE(CleanupAppShutdown, App);
	}
#undef FLOWUI_ERROR_SITE_CASE
	return {site, ErrorSubjectKind::None, "UnknownErrorSite"};
}

[[nodiscard]] constexpr std::string_view errorSiteName(ErrorSite site) noexcept {
	return describeErrorSite(site).name;
}

/**
 * Initial stable code space for known FlowUi error families.
 *
 * Values are explicitly grouped by domain. Once a value is part of a published
 * API it must not be reused for a different meaning. More specific codes can be
 * added without changing the compact FlowUiError representation.
 */
enum class ErrorCode : std::uint16_t {
	None = 0x0000,

	InternalInvariantBroken = 0x0101,
	IdentitySpaceExhausted = 0x0102,
	ArithmeticOverflow = 0x0103,
	AllocationFailed = 0x0104,
	ObjectNotInitialized = 0x0105,
	ObjectAlreadyInitialized = 0x0106,
	UnsupportedBuildFeature = 0x0107,
	WrongThread = 0x0108,
	CleanupIncomplete = 0x0109,
	ForeignExceptionObserved = 0x010a,

	AppUnavailable = 0x0201,
	AppInitializationFailed = 0x0202,
	AppTickSpaceExhausted = 0x0203,

	PlatformInitializationFailed = 0x0301,
	PlatformMonitorUnavailable = 0x0302,
	PlatformRequiredExtensionMissing = 0x0303,
	PlatformSurfaceCreationFailed = 0x0304,
	PlatformInputQueueOverflow = 0x0305,

	InvalidWindowId = 0x0401,
	InvalidWindowConfiguration = 0x0402,
	WindowCreationFailed = 0x0403,
	WindowIdSpaceExhausted = 0x0404,
	WindowRegistryCollision = 0x0405,
	MainWindowDestructionForbidden = 0x0406,
	WindowPresentationUnsupported = 0x0407,
	WindowRecreationFailed = 0x0408,

	FrameAlreadyActive = 0x0501,
	FramePhaseViolation = 0x0502,
	FrameNotReady = 0x0503,
	FrameNumberSpaceExhausted = 0x0504,
	PreparedFrameStale = 0x0505,
	FrameCapacityExceeded = 0x0506,
	UiBuildCallbackFailed = 0x0507,

	VulkanVersionUnsupported = 0x0601,
	VulkanExtensionMissing = 0x0602,
	VulkanDeviceUnavailable = 0x0603,
	VulkanFeatureMissing = 0x0604,
	VulkanNativeCallFailed = 0x0605,
	VulkanDeviceLost = 0x0606,
	SwapchainUnavailable = 0x0607,
	SwapchainOutOfDate = 0x0608,
	SwapchainImageInvalid = 0x0609,
	PresentationFailed = 0x060a,
	PresentIdSpaceExhausted = 0x060b,

	ShaderUnavailable = 0x0701,
	ShaderInvalid = 0x0702,
	RendererConfigurationInvalid = 0x0703,
	RendererCapacityExceeded = 0x0704,
	RenderCommandInvalid = 0x0705,
	RendererGenerationStale = 0x0706,
	RendererNativeResourceInvalid = 0x0707,

	StorageConfigurationInvalid = 0x0801,
	StorageBudgetExceeded = 0x0802,
	StorageCapacityExceeded = 0x0803,
	StorageMutationSealed = 0x0804,
	StorageFrameProtocolViolation = 0x0805,
	StorageSubmissionProtocolViolation = 0x0806,
	StoragePublicationFailed = 0x0807,
	StorageGenerationExhausted = 0x0808,

	InvalidResourceKey = 0x0901,
	ResourceNotFound = 0x0902,
	ResourceHandleInvalid = 0x0903,
	ResourceKindMismatch = 0x0904,
	ResourceWindowMismatch = 0x0905,
	ResourceCreationFailed = 0x0906,
	ResourcePublicationFailed = 0x0907,
	ResourceInUse = 0x0908,
	ResourceGenerationExhausted = 0x0909,

	AssetPathEmpty = 0x0a01,
	AssetNotFound = 0x0a02,
	AssetOpenFailed = 0x0a03,
	AssetReadFailed = 0x0a04,
	AssetFormatUnsupported = 0x0a05,
	AssetPayloadInvalid = 0x0a06,
	AssetDecodeFailed = 0x0a07,

	FontFamilyAlreadyExists = 0x0b01,
	FontFamilyNotFound = 0x0b02,
	FontBakeUnavailable = 0x0b03,
	FontBakeFailed = 0x0b04,
	FontAtlasCapacityExceeded = 0x0b05,
	FontIdSpaceExhausted = 0x0b06,
	DefaultFontUnavailable = 0x0b07,

	ImageDecodeFailed = 0x0c01,
	ImageSizeOverflow = 0x0c02,
	ImagePublicationFailed = 0x0c03,

	IconSourceInvalid = 0x0d01,
	IconNotFound = 0x0d02,
	IconRasterizationFailed = 0x0d03,
	IconRasterInvalid = 0x0d04,
	IconAtlasCapacityExceeded = 0x0d05,
	IconKeyCollision = 0x0d06,

	InvalidElementId = 0x0e01,
	ElementDefinitionConflict = 0x0e02,
	ElementStateUnavailable = 0x0e03,
	ElementResourceUnavailable = 0x0e04,
	ElementResourceConstructionFailed = 0x0e05,
	ElementResourceRecursiveConstruction = 0x0e06,
	ElementResourceDestroying = 0x0e07,
	ElementStorageStale = 0x0e08,

	InvalidActionId = 0x0f01,
	ActionAlreadyBound = 0x0f02,
	ActionNotBound = 0x0f03,
	ActionDisabled = 0x0f04,
	ActionResultTypeMismatch = 0x0f05,
	ActionPublicationConflict = 0x0f06,

	ThemeTypeNotFound = 0x1001,
	ThemeVariantNotFound = 0x1002,
	ThemeActiveVariantMissing = 0x1003,
	ThemeMutationFailed = 0x1004,

	InputFieldUnavailable = 0x1101,
	InputRangeInvalid = 0x1102,
	InputUtf8Invalid = 0x1103,
	InputSizeLimitExceeded = 0x1104,

	PopupFrameInactive = 0x1201,
	PopupDuplicateSubmission = 0x1202,
	PopupAnchorNotFound = 0x1203,
	PopupCapacityExceeded = 0x1204,

	ShortcutInvalid = 0x1301,
	ShortcutIdSpaceExhausted = 0x1302,
	ShortcutNotFound = 0x1303,

	ViewportDetached = 0x1401,
	ViewportConfigurationInvalid = 0x1402,
	ViewportNotFound = 0x1403,
	ViewportKeyCollision = 0x1404,
	ViewportGenerationIncomplete = 0x1405,
	ViewportGenerationExhausted = 0x1406,
	ViewportRecordingFailed = 0x1407,
};

/** Static catalog data associated with one ErrorCode. */
struct ErrorDescriptor {
	ErrorCode code = ErrorCode::None;
	ErrorDomain domain = ErrorDomain::None;
	ErrorCategory category = ErrorCategory::Local;
	ErrorBoundary boundary = ErrorBoundary::DirectCaller;
	ErrorResolvability resolvability = ErrorResolvability::CallerResolvable;
	ErrorImpact impact = ErrorImpact::None;
	ErrorResolutionPolicy resolutionPolicy = ErrorResolutionPolicy::CallerDefined;
	ErrorResolution defaultResolution = ErrorResolution::None;
	std::string_view name = "None";
	std::string_view message = "No FlowUi error.";
};

namespace detail {

[[nodiscard]] constexpr ErrorDomain errorDomainFromValue(ErrorCode code) noexcept {
	switch (static_cast<std::uint16_t>(code) >> 8u) {
	case 0x01: return ErrorDomain::Core;
	case 0x02: return ErrorDomain::App;
	case 0x03: return ErrorDomain::Platform;
	case 0x04: return ErrorDomain::Window;
	case 0x05: return ErrorDomain::Frame;
	case 0x06: return ErrorDomain::Vulkan;
	case 0x07: return ErrorDomain::Renderer;
	case 0x08: return ErrorDomain::Storage;
	case 0x09: return ErrorDomain::Resource;
	case 0x0a: return ErrorDomain::Asset;
	case 0x0b: return ErrorDomain::Font;
	case 0x0c: return ErrorDomain::Image;
	case 0x0d: return ErrorDomain::Icon;
	case 0x0e: return ErrorDomain::Element;
	case 0x0f: return ErrorDomain::Action;
	case 0x10: return ErrorDomain::Theme;
	case 0x11: return ErrorDomain::Input;
	case 0x12: return ErrorDomain::Popup;
	case 0x13: return ErrorDomain::Shortcut;
	case 0x14: return ErrorDomain::Viewport;
	default: return ErrorDomain::None;
	}
}

[[nodiscard]] constexpr ErrorDescriptor localDescriptor(
	ErrorCode code,
	std::string_view name,
	std::string_view message,
	ErrorBoundary boundary = ErrorBoundary::DirectCaller) noexcept {
	return ErrorDescriptor{
		.code = code,
		.domain = errorDomainFromValue(code),
		.category = ErrorCategory::Local,
		.boundary = boundary,
		.resolvability = ErrorResolvability::CallerResolvable,
		.impact = ErrorImpact::OperationFailed,
		.resolutionPolicy = ErrorResolutionPolicy::CallerDefined,
		.defaultResolution = ErrorResolution::Rejected,
		.name = name,
		.message = message,
	};
}

} // namespace detail

/** Return the initial descriptor for a stable error code. */
[[nodiscard]] constexpr ErrorDescriptor describeError(ErrorCode code) noexcept {
	using detail::localDescriptor;
	switch (code) {
	case ErrorCode::None:
		return {};

	case ErrorCode::InternalInvariantBroken:
		return {code, ErrorDomain::Core, ErrorCategory::Fatal, ErrorBoundary::Process,
			ErrorResolvability::NotResolvable, ErrorImpact::Fatal,
			ErrorResolutionPolicy::None, ErrorResolution::Terminated,
			"InternalInvariantBroken", "A required FlowUi internal invariant was broken."};
	case ErrorCode::IdentitySpaceExhausted:
		return {code, ErrorDomain::Core, ErrorCategory::Fatal, ErrorBoundary::Process,
			ErrorResolvability::NotResolvable, ErrorImpact::Fatal,
			ErrorResolutionPolicy::None, ErrorResolution::Terminated,
			"IdentitySpaceExhausted", "A non-reusable FlowUi identity space was exhausted."};
	case ErrorCode::ArithmeticOverflow:
		return localDescriptor(code, "ArithmeticOverflow", "A FlowUi size or offset calculation overflowed.");
	case ErrorCode::AllocationFailed:
		return {code, ErrorDomain::Core, ErrorCategory::Unwind, ErrorBoundary::AppLifecycle,
			ErrorResolvability::RecreateApp, ErrorImpact::AppUnavailable,
			ErrorResolutionPolicy::CallerDefined, ErrorResolution::Propagated,
			"AllocationFailed", "FlowUi could not allocate required memory."};
	case ErrorCode::ObjectNotInitialized:
		return localDescriptor(code, "ObjectNotInitialized", "The FlowUi object is not initialized.");
	case ErrorCode::ObjectAlreadyInitialized:
		return localDescriptor(code, "ObjectAlreadyInitialized", "The FlowUi object is already initialized.");
	case ErrorCode::UnsupportedBuildFeature:
		return {code, ErrorDomain::Core, ErrorCategory::Static, ErrorBoundary::CompileExpression,
			ErrorResolvability::NotResolvable, ErrorImpact::OperationFailed,
			ErrorResolutionPolicy::None, ErrorResolution::Rejected,
			"UnsupportedBuildFeature", "The requested FlowUi feature is not present in this build."};
	case ErrorCode::WrongThread:
		return localDescriptor(code, "WrongThread", "The operation was called from the wrong thread.");
	case ErrorCode::CleanupIncomplete:
		return {code, ErrorDomain::Core, ErrorCategory::Fatal, ErrorBoundary::Process,
			ErrorResolvability::NotResolvable, ErrorImpact::AppUnavailable,
			ErrorResolutionPolicy::HardCoded, ErrorResolution::Terminated,
			"CleanupIncomplete", "FlowUi could not complete required cleanup."};
	case ErrorCode::ForeignExceptionObserved:
		return {code, ErrorDomain::Core, ErrorCategory::Unwind, ErrorBoundary::AppLifecycle,
			ErrorResolvability::CallerResolvable, ErrorImpact::OperationFailed,
			ErrorResolutionPolicy::CallerDefined, ErrorResolution::Propagated,
			"ForeignExceptionObserved", "User or third-party code threw while FlowUi was active."};

	case ErrorCode::AppUnavailable:
		return localDescriptor(code, "AppUnavailable", "The FlowUi application is unavailable.");
	case ErrorCode::AppInitializationFailed:
		return {code, ErrorDomain::App, ErrorCategory::Unwind, ErrorBoundary::AppLifecycle,
			ErrorResolvability::CallerResolvable, ErrorImpact::AppUnavailable,
			ErrorResolutionPolicy::CallerDefined, ErrorResolution::Propagated,
			"AppInitializationFailed", "FlowUi application initialization failed."};
	case ErrorCode::AppTickSpaceExhausted:
		return {code, ErrorDomain::App, ErrorCategory::Fatal, ErrorBoundary::Process,
			ErrorResolvability::NotResolvable, ErrorImpact::Fatal,
			ErrorResolutionPolicy::None, ErrorResolution::Terminated,
			"AppTickSpaceExhausted", "The FlowUi application tick identity space was exhausted."};

	case ErrorCode::PlatformInitializationFailed:
		return {code, ErrorDomain::Platform, ErrorCategory::Unwind, ErrorBoundary::AppLifecycle,
			ErrorResolvability::CallerResolvable, ErrorImpact::AppUnavailable,
			ErrorResolutionPolicy::CallerDefined, ErrorResolution::Propagated,
			"PlatformInitializationFailed", "The platform window system could not be initialized."};
	case ErrorCode::PlatformMonitorUnavailable:
		return localDescriptor(code, "PlatformMonitorUnavailable", "The requested platform monitor is unavailable.");
	case ErrorCode::PlatformRequiredExtensionMissing:
		return {code, ErrorDomain::Platform, ErrorCategory::Unwind, ErrorBoundary::AppLifecycle,
			ErrorResolvability::CallerResolvable, ErrorImpact::AppUnavailable,
			ErrorResolutionPolicy::CallerDefined, ErrorResolution::Propagated,
			"PlatformRequiredExtensionMissing", "A required platform extension is unavailable."};
	case ErrorCode::PlatformSurfaceCreationFailed:
		return {code, ErrorDomain::Platform, ErrorCategory::Unwind, ErrorBoundary::WindowLifecycle,
			ErrorResolvability::CallerResolvable, ErrorImpact::WindowUnavailable,
			ErrorResolutionPolicy::CallerDefined, ErrorResolution::Propagated,
			"PlatformSurfaceCreationFailed", "The platform rendering surface could not be created."};
	case ErrorCode::PlatformInputQueueOverflow:
		return {code, ErrorDomain::Platform, ErrorCategory::Local, ErrorBoundary::ManagerOperation,
			ErrorResolvability::AutomaticallyResolved, ErrorImpact::Degraded,
			ErrorResolutionPolicy::AppConfigured, ErrorResolution::Ignored,
			"PlatformInputQueueOverflow", "The bounded platform input queue overflowed."};

	case ErrorCode::InvalidWindowId: return localDescriptor(code, "InvalidWindowId", "The window identity is invalid or no longer registered.");
	case ErrorCode::InvalidWindowConfiguration: return localDescriptor(code, "InvalidWindowConfiguration", "The window configuration is invalid.");
	case ErrorCode::WindowCreationFailed: return localDescriptor(code, "WindowCreationFailed", "The FlowUi window could not be created.", ErrorBoundary::WindowLifecycle);
	case ErrorCode::WindowIdSpaceExhausted:
	case ErrorCode::WindowRegistryCollision:
		return {code, ErrorDomain::Window, ErrorCategory::Fatal, ErrorBoundary::Process,
			ErrorResolvability::NotResolvable, ErrorImpact::Fatal,
			ErrorResolutionPolicy::None, ErrorResolution::Terminated,
			code == ErrorCode::WindowIdSpaceExhausted ? "WindowIdSpaceExhausted" : "WindowRegistryCollision",
			code == ErrorCode::WindowIdSpaceExhausted ? "The FlowUi window identity space was exhausted." : "The FlowUi window registry detected an identity collision."};
	case ErrorCode::MainWindowDestructionForbidden: return localDescriptor(code, "MainWindowDestructionForbidden", "The semantic main window cannot be destroyed explicitly.");
	case ErrorCode::WindowPresentationUnsupported: return localDescriptor(code, "WindowPresentationUnsupported", "The selected device cannot present to this window.", ErrorBoundary::WindowLifecycle);
	case ErrorCode::WindowRecreationFailed:
		return {code, ErrorDomain::Window, ErrorCategory::Unwind, ErrorBoundary::WindowLifecycle,
			ErrorResolvability::RecreateWindow, ErrorImpact::WindowUnavailable,
			ErrorResolutionPolicy::CallerDefined, ErrorResolution::Propagated,
			"WindowRecreationFailed", "The FlowUi window rendering resources could not be recreated."};

	case ErrorCode::FrameAlreadyActive: return localDescriptor(code, "FrameAlreadyActive", "Another FlowUi window frame is already active.");
	case ErrorCode::FramePhaseViolation: return localDescriptor(code, "FramePhaseViolation", "The frame operation is invalid in the current phase.");
	case ErrorCode::FrameNotReady: return localDescriptor(code, "FrameNotReady", "The FlowUi window is not ready for the requested frame operation.");
	case ErrorCode::FrameNumberSpaceExhausted:
		return {code, ErrorDomain::Frame, ErrorCategory::Fatal, ErrorBoundary::Process,
			ErrorResolvability::NotResolvable, ErrorImpact::Fatal,
			ErrorResolutionPolicy::None, ErrorResolution::Terminated,
			"FrameNumberSpaceExhausted", "The FlowUi frame identity space was exhausted."};
	case ErrorCode::PreparedFrameStale:
		return {code, ErrorDomain::Frame, ErrorCategory::Fatal, ErrorBoundary::WindowFrame,
			ErrorResolvability::NotResolvable, ErrorImpact::Fatal,
			ErrorResolutionPolicy::None, ErrorResolution::Halted,
			"PreparedFrameStale", "Prepared UI data does not match its sealed storage frame."};
	case ErrorCode::FrameCapacityExceeded:
		return {code, ErrorDomain::Frame, ErrorCategory::Unwind, ErrorBoundary::WindowFrame,
			ErrorResolvability::CallerResolvable, ErrorImpact::FrameCanceled,
			ErrorResolutionPolicy::CallerDefined, ErrorResolution::Propagated,
			"FrameCapacityExceeded", "The frame exceeded a configured FlowUi capacity."};
	case ErrorCode::UiBuildCallbackFailed:
		return {code, ErrorDomain::Frame, ErrorCategory::Local, ErrorBoundary::WindowFrame,
			ErrorResolvability::AutomaticallyResolved, ErrorImpact::FrameCanceled,
			ErrorResolutionPolicy::HardCoded, ErrorResolution::CanceledFrame,
			"UiBuildCallbackFailed", "A managed window UI build callback threw an exception."};

	case ErrorCode::VulkanVersionUnsupported:
	case ErrorCode::VulkanExtensionMissing:
	case ErrorCode::VulkanDeviceUnavailable:
	case ErrorCode::VulkanFeatureMissing:
		return {code, ErrorDomain::Vulkan, ErrorCategory::Unwind, ErrorBoundary::AppLifecycle,
			ErrorResolvability::CallerResolvable, ErrorImpact::AppUnavailable,
			ErrorResolutionPolicy::CallerDefined, ErrorResolution::Propagated,
			code == ErrorCode::VulkanVersionUnsupported ? "VulkanVersionUnsupported" :
				(code == ErrorCode::VulkanExtensionMissing ? "VulkanExtensionMissing" :
				(code == ErrorCode::VulkanDeviceUnavailable ? "VulkanDeviceUnavailable" : "VulkanFeatureMissing")),
			code == ErrorCode::VulkanVersionUnsupported ? "The required Vulkan version is unavailable." :
				(code == ErrorCode::VulkanExtensionMissing ? "A required Vulkan extension is unavailable." :
				(code == ErrorCode::VulkanDeviceUnavailable ? "No compatible Vulkan device is available." : "The selected Vulkan device lacks a required feature."))};
	case ErrorCode::VulkanNativeCallFailed:
		return {code, ErrorDomain::Vulkan, ErrorCategory::Unwind, ErrorBoundary::WindowFrame,
			ErrorResolvability::CallerResolvable, ErrorImpact::FrameCanceled,
			ErrorResolutionPolicy::CallerDefined, ErrorResolution::Propagated,
			"VulkanNativeCallFailed", "A Vulkan operation failed."};
	case ErrorCode::VulkanDeviceLost:
		return {code, ErrorDomain::Vulkan, ErrorCategory::Unwind, ErrorBoundary::AppLifecycle,
			ErrorResolvability::RecreateApp, ErrorImpact::AppUnavailable,
			ErrorResolutionPolicy::CallerDefined, ErrorResolution::Propagated,
			"VulkanDeviceLost", "The Vulkan device was lost."};
	case ErrorCode::SwapchainUnavailable: return localDescriptor(code, "SwapchainUnavailable", "A usable swapchain could not be created.", ErrorBoundary::WindowLifecycle);
	case ErrorCode::SwapchainOutOfDate:
		return {code, ErrorDomain::Vulkan, ErrorCategory::Local, ErrorBoundary::WindowLifecycle,
			ErrorResolvability::AutomaticallyResolved, ErrorImpact::FrameCanceled,
			ErrorResolutionPolicy::HardCoded, ErrorResolution::RecreatedWindow,
			"SwapchainOutOfDate", "The window swapchain is out of date."};
	case ErrorCode::SwapchainImageInvalid:
		return {code, ErrorDomain::Vulkan, ErrorCategory::Fatal, ErrorBoundary::Process,
			ErrorResolvability::NotResolvable, ErrorImpact::Fatal,
			ErrorResolutionPolicy::None, ErrorResolution::Terminated,
			"SwapchainImageInvalid", "The acquired swapchain image is inconsistent with FlowUi state."};
	case ErrorCode::PresentationFailed:
		return {code, ErrorDomain::Vulkan, ErrorCategory::Unwind, ErrorBoundary::WindowLifecycle,
			ErrorResolvability::RecreateWindow, ErrorImpact::WindowUnavailable,
			ErrorResolutionPolicy::CallerDefined, ErrorResolution::Propagated,
			"PresentationFailed", "Presentation of a FlowUi window failed."};
	case ErrorCode::PresentIdSpaceExhausted:
		return {code, ErrorDomain::Vulkan, ErrorCategory::Fatal, ErrorBoundary::Process,
			ErrorResolvability::NotResolvable, ErrorImpact::Fatal,
			ErrorResolutionPolicy::None, ErrorResolution::Terminated,
			"PresentIdSpaceExhausted", "The swapchain presentation identity space was exhausted."};

	case ErrorCode::ShaderUnavailable:
	case ErrorCode::ShaderInvalid:
	case ErrorCode::RendererConfigurationInvalid:
		return {code, ErrorDomain::Renderer, ErrorCategory::Unwind, ErrorBoundary::AppLifecycle,
			ErrorResolvability::CallerResolvable, ErrorImpact::AppUnavailable,
			ErrorResolutionPolicy::CallerDefined, ErrorResolution::Propagated,
			code == ErrorCode::ShaderUnavailable ? "ShaderUnavailable" :
				(code == ErrorCode::ShaderInvalid ? "ShaderInvalid" : "RendererConfigurationInvalid"),
			code == ErrorCode::ShaderUnavailable ? "A required renderer shader is unavailable." :
				(code == ErrorCode::ShaderInvalid ? "A renderer shader payload is invalid." : "The renderer configuration is invalid.")};
	case ErrorCode::RendererCapacityExceeded:
		return {code, ErrorDomain::Renderer, ErrorCategory::Unwind, ErrorBoundary::WindowFrame,
			ErrorResolvability::CallerResolvable, ErrorImpact::FrameCanceled,
			ErrorResolutionPolicy::CallerDefined, ErrorResolution::Propagated,
			"RendererCapacityExceeded", "The renderer exceeded a configured capacity."};
	case ErrorCode::RenderCommandInvalid:
		return {code, ErrorDomain::Renderer, ErrorCategory::Unwind, ErrorBoundary::WindowFrame,
			ErrorResolvability::CallerResolvable, ErrorImpact::FrameCanceled,
			ErrorResolutionPolicy::CallerDefined, ErrorResolution::Propagated,
			"RenderCommandInvalid", "The UI render command stream is invalid."};
	case ErrorCode::RendererGenerationStale:
	case ErrorCode::RendererNativeResourceInvalid:
		return {code, ErrorDomain::Renderer, ErrorCategory::Fatal, ErrorBoundary::Process,
			ErrorResolvability::NotResolvable, ErrorImpact::Fatal,
			ErrorResolutionPolicy::None, ErrorResolution::Terminated,
			code == ErrorCode::RendererGenerationStale ? "RendererGenerationStale" : "RendererNativeResourceInvalid",
			code == ErrorCode::RendererGenerationStale ? "The renderer received a stale resource generation." : "The renderer received invalid committed native resources."};

	case ErrorCode::StorageConfigurationInvalid: return localDescriptor(code, "StorageConfigurationInvalid", "The storage configuration is invalid.");
	case ErrorCode::StorageBudgetExceeded:
	case ErrorCode::StorageCapacityExceeded:
		return {code, ErrorDomain::Storage, ErrorCategory::Local, ErrorBoundary::DirectCaller,
			ErrorResolvability::CallerResolvable, ErrorImpact::OperationFailed,
			ErrorResolutionPolicy::AppConfigured, ErrorResolution::Rejected,
			code == ErrorCode::StorageBudgetExceeded ? "StorageBudgetExceeded" : "StorageCapacityExceeded",
			code == ErrorCode::StorageBudgetExceeded ? "A FlowUi storage budget was exceeded." : "A FlowUi storage capacity was exceeded."};
	case ErrorCode::StorageMutationSealed: return localDescriptor(code, "StorageMutationSealed", "Storage mutation is not allowed while a sealed snapshot is active.");
	case ErrorCode::StorageFrameProtocolViolation:
		return localDescriptor(code, "StorageFrameProtocolViolation",
			"The storage frame operation violates the current frame state.", ErrorBoundary::WindowFrame);
	case ErrorCode::StorageSubmissionProtocolViolation:
		return {code, ErrorDomain::Storage, ErrorCategory::Fatal, ErrorBoundary::Process,
			ErrorResolvability::NotResolvable, ErrorImpact::Fatal,
			ErrorResolutionPolicy::None, ErrorResolution::Terminated,
			"StorageSubmissionProtocolViolation", "The storage submission ownership protocol was violated."};
	case ErrorCode::StoragePublicationFailed: return localDescriptor(code, "StoragePublicationFailed", "Storage could not publish the requested record or resource.");
	case ErrorCode::StorageGenerationExhausted:
		return {code, ErrorDomain::Storage, ErrorCategory::Fatal, ErrorBoundary::Process,
			ErrorResolvability::NotResolvable, ErrorImpact::Fatal,
			ErrorResolutionPolicy::None, ErrorResolution::Terminated,
			"StorageGenerationExhausted", "A storage generation space was exhausted."};

	case ErrorCode::InvalidResourceKey: return localDescriptor(code, "InvalidResourceKey", "The resource key is invalid for this manager.");
	case ErrorCode::ResourceNotFound: return localDescriptor(code, "ResourceNotFound", "The requested FlowUi resource was not found.");
	case ErrorCode::ResourceHandleInvalid: return localDescriptor(code, "ResourceHandleInvalid", "The FlowUi resource handle is invalid or stale.");
	case ErrorCode::ResourceKindMismatch: return localDescriptor(code, "ResourceKindMismatch", "The FlowUi resource has the wrong kind.");
	case ErrorCode::ResourceWindowMismatch: return localDescriptor(code, "ResourceWindowMismatch", "The FlowUi resource belongs to a different window.");
	case ErrorCode::ResourceCreationFailed: return localDescriptor(code, "ResourceCreationFailed", "The FlowUi resource could not be created.");
	case ErrorCode::ResourcePublicationFailed: return localDescriptor(code, "ResourcePublicationFailed", "The FlowUi resource could not be published.");
	case ErrorCode::ResourceInUse: return localDescriptor(code, "ResourceInUse", "The FlowUi resource is still in use.");
	case ErrorCode::ResourceGenerationExhausted:
		return {code, ErrorDomain::Resource, ErrorCategory::Fatal, ErrorBoundary::Process,
			ErrorResolvability::NotResolvable, ErrorImpact::Fatal,
			ErrorResolutionPolicy::None, ErrorResolution::Terminated,
			"ResourceGenerationExhausted", "A resource generation space was exhausted."};

	case ErrorCode::AssetPathEmpty: return localDescriptor(code, "AssetPathEmpty", "The asset path is empty.");
	case ErrorCode::AssetNotFound: return localDescriptor(code, "AssetNotFound", "The requested asset does not exist.");
	case ErrorCode::AssetOpenFailed: return localDescriptor(code, "AssetOpenFailed", "The asset could not be opened.");
	case ErrorCode::AssetReadFailed: return localDescriptor(code, "AssetReadFailed", "The asset could not be read.");
	case ErrorCode::AssetFormatUnsupported: return localDescriptor(code, "AssetFormatUnsupported", "The asset format is unsupported.");
	case ErrorCode::AssetPayloadInvalid: return localDescriptor(code, "AssetPayloadInvalid", "The asset payload is invalid.");
	case ErrorCode::AssetDecodeFailed: return localDescriptor(code, "AssetDecodeFailed", "The asset could not be decoded.");

	case ErrorCode::FontFamilyAlreadyExists: return localDescriptor(code, "FontFamilyAlreadyExists", "The font family is already registered.");
	case ErrorCode::FontFamilyNotFound: return localDescriptor(code, "FontFamilyNotFound", "The font family is not registered.");
	case ErrorCode::FontBakeUnavailable: return localDescriptor(code, "FontBakeUnavailable", "Runtime font baking is unavailable in this build.");
	case ErrorCode::FontBakeFailed: return localDescriptor(code, "FontBakeFailed", "Runtime font baking failed.");
	case ErrorCode::FontAtlasCapacityExceeded:
		return {code, ErrorDomain::Font, ErrorCategory::Local, ErrorBoundary::ManagerOperation,
			ErrorResolvability::CallerResolvable, ErrorImpact::OperationFailed,
			ErrorResolutionPolicy::AppConfigured, ErrorResolution::Rejected,
			"FontAtlasCapacityExceeded", "The font atlas capacity was exceeded."};
	case ErrorCode::FontIdSpaceExhausted: return localDescriptor(code, "FontIdSpaceExhausted", "The font identity space was exhausted.", ErrorBoundary::ManagerOperation);
	case ErrorCode::DefaultFontUnavailable:
		return {code, ErrorDomain::Font, ErrorCategory::Local, ErrorBoundary::ManagerOperation,
			ErrorResolvability::CallerResolvable, ErrorImpact::Degraded,
			ErrorResolutionPolicy::AppConfigured, ErrorResolution::UsedFallback,
			"DefaultFontUnavailable", "No configured default font could be loaded."};

	case ErrorCode::ImageDecodeFailed: return localDescriptor(code, "ImageDecodeFailed", "The image could not be decoded.");
	case ErrorCode::ImageSizeOverflow: return localDescriptor(code, "ImageSizeOverflow", "The decoded image is too large to address.");
	case ErrorCode::ImagePublicationFailed: return localDescriptor(code, "ImagePublicationFailed", "The decoded image could not be published.");

	case ErrorCode::IconSourceInvalid: return localDescriptor(code, "IconSourceInvalid", "The icon source is invalid.");
	case ErrorCode::IconNotFound: return localDescriptor(code, "IconNotFound", "The requested icon is not registered.");
	case ErrorCode::IconRasterizationFailed: return localDescriptor(code, "IconRasterizationFailed", "The icon could not be rasterized.");
	case ErrorCode::IconRasterInvalid: return localDescriptor(code, "IconRasterInvalid", "The icon raster payload is invalid.");
	case ErrorCode::IconAtlasCapacityExceeded:
		return {code, ErrorDomain::Icon, ErrorCategory::Local, ErrorBoundary::ManagerOperation,
			ErrorResolvability::CallerResolvable, ErrorImpact::Degraded,
			ErrorResolutionPolicy::AppConfigured, ErrorResolution::UsedFallback,
			"IconAtlasCapacityExceeded", "The icon atlas has no usable capacity."};
	case ErrorCode::IconKeyCollision:
		return {code, ErrorDomain::Icon, ErrorCategory::Fatal, ErrorBoundary::Process,
			ErrorResolvability::NotResolvable, ErrorImpact::Fatal,
			ErrorResolutionPolicy::None, ErrorResolution::Terminated,
			"IconKeyCollision", "An internally generated icon key collided."};

	case ErrorCode::InvalidElementId: return localDescriptor(code, "InvalidElementId", "The Flow element identity is invalid.");
	case ErrorCode::ElementDefinitionConflict: return localDescriptor(code, "ElementDefinitionConflict", "The Flow element definition conflicts with an existing definition.", ErrorBoundary::ManagerOperation);
	case ErrorCode::ElementStateUnavailable: return localDescriptor(code, "ElementStateUnavailable", "The Flow element state is unavailable.");
	case ErrorCode::ElementResourceUnavailable: return localDescriptor(code, "ElementResourceUnavailable", "The Flow element resource is unavailable.");
	case ErrorCode::ElementResourceConstructionFailed:
		return {code, ErrorDomain::Element, ErrorCategory::Unwind, ErrorBoundary::WindowFrame,
			ErrorResolvability::CallerResolvable, ErrorImpact::FrameCanceled,
			ErrorResolutionPolicy::CallerDefined, ErrorResolution::Propagated,
			"ElementResourceConstructionFailed", "Flow element resource construction failed."};
	case ErrorCode::ElementResourceRecursiveConstruction: return localDescriptor(code, "ElementResourceRecursiveConstruction", "Flow element resource construction is recursive.");
	case ErrorCode::ElementResourceDestroying: return localDescriptor(code, "ElementResourceDestroying", "Flow element resources are being destroyed.");
	case ErrorCode::ElementStorageStale:
		return {code, ErrorDomain::Element, ErrorCategory::Fatal, ErrorBoundary::Process,
			ErrorResolvability::NotResolvable, ErrorImpact::Fatal,
			ErrorResolutionPolicy::None, ErrorResolution::Terminated,
			"ElementStorageStale", "Committed Flow element storage became stale."};

	case ErrorCode::InvalidActionId: return localDescriptor(code, "InvalidActionId", "The action identity is invalid.");
	case ErrorCode::ActionAlreadyBound: return localDescriptor(code, "ActionAlreadyBound", "The action is already bound.");
	case ErrorCode::ActionNotBound: return localDescriptor(code, "ActionNotBound", "The action is not bound.");
	case ErrorCode::ActionDisabled: return localDescriptor(code, "ActionDisabled", "The action is disabled.");
	case ErrorCode::ActionResultTypeMismatch: return localDescriptor(code, "ActionResultTypeMismatch", "The action result type does not match the invocation.");
	case ErrorCode::ActionPublicationConflict:
		return {code, ErrorDomain::Action, ErrorCategory::Fatal, ErrorBoundary::Process,
			ErrorResolvability::NotResolvable, ErrorImpact::Fatal,
			ErrorResolutionPolicy::None, ErrorResolution::Terminated,
			"ActionPublicationConflict", "Action binding state changed during publication."};

	case ErrorCode::ThemeTypeNotFound: return localDescriptor(code, "ThemeTypeNotFound", "The requested theme type is not registered.");
	case ErrorCode::ThemeVariantNotFound: return localDescriptor(code, "ThemeVariantNotFound", "The requested theme variant is not registered.");
	case ErrorCode::ThemeActiveVariantMissing: return localDescriptor(code, "ThemeActiveVariantMissing", "The theme type has no active variant.");
	case ErrorCode::ThemeMutationFailed:
		return {code, ErrorDomain::Theme, ErrorCategory::Unwind, ErrorBoundary::AppPoll,
			ErrorResolvability::CallerResolvable, ErrorImpact::OperationFailed,
			ErrorResolutionPolicy::CallerDefined, ErrorResolution::Propagated,
			"ThemeMutationFailed", "A staged theme mutation failed."};

	case ErrorCode::InputFieldUnavailable: return localDescriptor(code, "InputFieldUnavailable", "The requested input field is unavailable.");
	case ErrorCode::InputRangeInvalid: return localDescriptor(code, "InputRangeInvalid", "The input edit range is invalid.");
	case ErrorCode::InputUtf8Invalid: return localDescriptor(code, "InputUtf8Invalid", "The input text is not valid UTF-8.");
	case ErrorCode::InputSizeLimitExceeded: return localDescriptor(code, "InputSizeLimitExceeded", "The input text exceeds its configured size limit.");

	case ErrorCode::PopupFrameInactive: return localDescriptor(code, "PopupFrameInactive", "The popup operation requires an active UI frame.");
	case ErrorCode::PopupDuplicateSubmission:
		return {code, ErrorDomain::Popup, ErrorCategory::Local, ErrorBoundary::ManagerOperation,
			ErrorResolvability::CallerResolvable, ErrorImpact::Degraded,
			ErrorResolutionPolicy::AppConfigured, ErrorResolution::Rejected,
			"PopupDuplicateSubmission", "The popup was submitted more than once in one frame."};
	case ErrorCode::PopupAnchorNotFound:
		return {code, ErrorDomain::Popup, ErrorCategory::Local, ErrorBoundary::ManagerOperation,
			ErrorResolvability::AutomaticallyResolved, ErrorImpact::Degraded,
			ErrorResolutionPolicy::AppConfigured, ErrorResolution::Skipped,
			"PopupAnchorNotFound", "The popup anchor was not found."};
	case ErrorCode::PopupCapacityExceeded:
		return {code, ErrorDomain::Popup, ErrorCategory::Local, ErrorBoundary::ManagerOperation,
			ErrorResolvability::AutomaticallyResolved, ErrorImpact::Degraded,
			ErrorResolutionPolicy::AppConfigured, ErrorResolution::Skipped,
			"PopupCapacityExceeded", "The popup capacity was exceeded."};

	case ErrorCode::ShortcutInvalid: return localDescriptor(code, "ShortcutInvalid", "The shortcut registration is invalid.");
	case ErrorCode::ShortcutIdSpaceExhausted: return localDescriptor(code, "ShortcutIdSpaceExhausted", "The shortcut identity space was exhausted.", ErrorBoundary::ManagerOperation);
	case ErrorCode::ShortcutNotFound: return localDescriptor(code, "ShortcutNotFound", "The shortcut registration was not found.");

	case ErrorCode::ViewportDetached: return localDescriptor(code, "ViewportDetached", "The viewport is detached from storage.");
	case ErrorCode::ViewportConfigurationInvalid: return localDescriptor(code, "ViewportConfigurationInvalid", "The viewport configuration is invalid.");
	case ErrorCode::ViewportNotFound:
		return {code, ErrorDomain::Viewport, ErrorCategory::Local, ErrorBoundary::ManagerOperation,
			ErrorResolvability::AutomaticallyResolved, ErrorImpact::Degraded,
			ErrorResolutionPolicy::AppConfigured, ErrorResolution::UsedFallback,
			"ViewportNotFound", "The requested viewport was not found."};
	case ErrorCode::ViewportKeyCollision:
		return localDescriptor(code, "ViewportKeyCollision", "The viewport key is already registered.", ErrorBoundary::ManagerOperation);
	case ErrorCode::ViewportGenerationIncomplete:
	case ErrorCode::ViewportGenerationExhausted:
		return {code, ErrorDomain::Viewport, ErrorCategory::Fatal, ErrorBoundary::Process,
			ErrorResolvability::NotResolvable, ErrorImpact::Fatal,
			ErrorResolutionPolicy::None, ErrorResolution::Terminated,
			code == ErrorCode::ViewportGenerationIncomplete ? "ViewportGenerationIncomplete" : "ViewportGenerationExhausted",
			code == ErrorCode::ViewportGenerationIncomplete ? "A published viewport generation is incomplete." : "The viewport generation identity space was exhausted."};
	case ErrorCode::ViewportRecordingFailed:
		return {code, ErrorDomain::Viewport, ErrorCategory::Unwind, ErrorBoundary::WindowFrame,
			ErrorResolvability::CallerResolvable, ErrorImpact::FrameCanceled,
			ErrorResolutionPolicy::CallerDefined, ErrorResolution::Propagated,
			"ViewportRecordingFailed", "Viewport command recording failed."};
	}
	return {code, detail::errorDomainFromValue(code), ErrorCategory::Fatal,
		ErrorBoundary::Process, ErrorResolvability::NotResolvable, ErrorImpact::Fatal,
		ErrorResolutionPolicy::None, ErrorResolution::Terminated,
		"UnknownErrorCode", "An unknown FlowUi error code was observed."};
}

[[nodiscard]] constexpr std::string_view errorName(ErrorCode code) noexcept {
	return describeError(code).name;
}

[[nodiscard]] constexpr std::string_view errorMessage(ErrorCode code) noexcept {
	return describeError(code).message;
}

/**
 * Small, allocation-free production error value.
 *
 * Debug names, source locations, stack traces, and state snapshots belong to the
 * optional development sidecar keyed by occurrence.
 */
struct FlowUiError final {
	std::uint64_t subject = 0u;
	std::uint64_t auxiliary = 0u;
	std::uint32_t nativeCode = 0u;
	ErrorCode code = ErrorCode::None;
	ErrorSite site = ErrorSite::None;

	[[nodiscard]] constexpr bool hasError() const noexcept {
		return code != ErrorCode::None;
	}

	[[nodiscard]] constexpr explicit operator bool() const noexcept {
		return hasError();
	}

	[[nodiscard]] constexpr ErrorDescriptor descriptor() const noexcept {
		return describeError(code);
	}

	[[nodiscard]] constexpr ErrorSiteDescriptor siteDescriptor() const noexcept {
		return describeErrorSite(site);
	}

	[[nodiscard]] constexpr ErrorDomain domain() const noexcept {
		return detail::errorDomainFromValue(code);
	}

	[[nodiscard]] constexpr ErrorSubjectKind subjectKind() const noexcept {
		return siteDescriptor().expectedSubject;
	}

	[[nodiscard]] constexpr std::string_view message() const noexcept {
		return errorMessage(code);
	}
};

static_assert(std::is_trivially_copyable_v<FlowUiError>);
static_assert(std::is_trivially_destructible_v<FlowUiError>);
static_assert(sizeof(FlowUiError) == 24u);

/** Construct a production error without allocating or formatting text. */
[[nodiscard]] constexpr FlowUiError makeError(
	ErrorCode code,
	ErrorSite site,
	std::uint64_t subject = 0u,
	std::uint64_t auxiliary = 0u,
	std::uint32_t nativeCode = 0u) noexcept {
	return FlowUiError{
		.subject = subject,
		.auxiliary = auxiliary,
		.nativeCode = nativeCode,
		.code = code,
		.site = site,
	};
}

#if defined(__cpp_lib_expected) && __cpp_lib_expected >= 202202L

/** Canonical local-failure result. */
template <typename T>
using Result = std::expected<T, FlowUiError>;

/** Canonical no-value local-failure result. */
using Status = Result<void>;

[[nodiscard]] constexpr std::unexpected<FlowUiError> unexpectedError(FlowUiError error) noexcept {
	return std::unexpected<FlowUiError>(error);
}

#else

namespace detail {
struct UnexpectedFlowUiError {
	FlowUiError error{};
};
} // namespace detail

/** Compatibility result used only when the selected C++ library lacks expected. */
template <typename T>
class Result {
public:
	Result() requires std::is_default_constructible_v<T> : storage_(T{}) {}
	Result(const T& value) : storage_(value) {}
	Result(T&& value) : storage_(std::move(value)) {}
	Result(detail::UnexpectedFlowUiError failure) : storage_(failure.error) {}

	[[nodiscard]] bool has_value() const noexcept { return storage_.index() == 0u; }
	[[nodiscard]] explicit operator bool() const noexcept { return has_value(); }
	[[nodiscard]] T& value() & { return std::get<0>(storage_); }
	[[nodiscard]] const T& value() const& { return std::get<0>(storage_); }
	[[nodiscard]] T&& value() && { return std::get<0>(std::move(storage_)); }
	[[nodiscard]] T& operator*() & { return value(); }
	[[nodiscard]] const T& operator*() const& { return value(); }
	[[nodiscard]] T* operator->() { return &value(); }
	[[nodiscard]] const T* operator->() const { return &value(); }
	[[nodiscard]] FlowUiError& error() & { return std::get<1>(storage_); }
	[[nodiscard]] const FlowUiError& error() const& { return std::get<1>(storage_); }

	template <typename U>
	[[nodiscard]] T value_or(U&& fallback) const& {
		return has_value() ? std::get<0>(storage_) : static_cast<T>(std::forward<U>(fallback));
	}

private:
	std::variant<T, FlowUiError> storage_;
};

template <>
class Result<void> {
public:
	Result() = default;
	Result(detail::UnexpectedFlowUiError failure) : error_(failure.error), hasValue_(false) {}
	[[nodiscard]] bool has_value() const noexcept { return hasValue_; }
	[[nodiscard]] explicit operator bool() const noexcept { return has_value(); }
	void value() const {
		if (!hasValue_) throw std::bad_variant_access{};
	}
	[[nodiscard]] FlowUiError& error() & { return error_; }
	[[nodiscard]] const FlowUiError& error() const& { return error_; }

private:
	FlowUiError error_{};
	bool hasValue_ = true;
};

using Status = Result<void>;

[[nodiscard]] constexpr detail::UnexpectedFlowUiError unexpectedError(FlowUiError error) noexcept {
	return detail::UnexpectedFlowUiError{error};
}

#endif

/** Typed distant-unwind carrier for a FlowUi-authored error. */
class FlowUiException final : public std::exception {
public:
	explicit FlowUiException(FlowUiError error) noexcept : error_(error) {}

	[[nodiscard]] const FlowUiError& error() const noexcept { return error_; }

	[[nodiscard]] const char* what() const noexcept override {
		// All descriptor messages are null-terminated string literals.
		return errorMessage(error_.code).data();
	}

private:
	FlowUiError error_{};
};

/** What in-process evidence access remains trustworthy after a fatal error. */
enum class FatalInspectionCapability : std::uint8_t {
	None = 0,
	TerminalOnly,
	ImmutableSnapshot,
	ReadOnlyInterface,
};

/** Why an error occurrence entered the centralized reporting path. */
enum class ErrorEventKind : std::uint8_t {
	Resolved = 0,
	Reported = 1,
	Fatal = 2,
	BackendDiagnostic = 3,
};

/**
 * Allocation-free report delivered to the configured sink and default writer.
 * nativeMessage is borrowed and remains valid only for the duration of a sink
 * callback. It is empty for ordinary FlowUi errors.
 */
struct ErrorEventView {
	FlowUiError error{};
	ErrorEventKind kind = ErrorEventKind::Reported;
	ErrorResolution resolution = ErrorResolution::None;
	FatalInspectionCapability inspection = FatalInspectionCapability::None;
	std::string_view nativeMessage{};
	std::uint32_t nativeCategory = 0u;
};

using ErrorSinkCallback = void(*)(void* userData, const ErrorEventView& event) noexcept;

/** Non-owning application-provided destination for structured error reports. */
struct ErrorSink {
	void* userData = nullptr;
	ErrorSinkCallback callback = nullptr;

	[[nodiscard]] constexpr explicit operator bool() const noexcept {
		return callback != nullptr;
	}

	void notify(const ErrorEventView& event) const noexcept {
		if (callback) callback(userData, event);
	}
};

/** How the hidden App observer routes each report. */
enum class ErrorReportingMode : std::uint8_t {
	/** Attempt compact default output first, then notify the sink when present. */
	SinkAndDefault = 0,
	/** Notify the sink when present; otherwise use compact default output. */
	SinkOrDefault = 1,
	/** Notify only the sink. An empty sink intentionally produces no output. */
	SinkOnly = 2,
	/** Ignore the sink and use only compact default output. */
	DefaultOnly = 3,
};

/** Immutable reporting configuration copied into the App-owned observer. */
struct ErrorObserverConfig {
	ErrorSink sink{};
	ErrorReportingMode mode = ErrorReportingMode::SinkOrDefault;
	/** Non-owning stream used by the compact built-in writer. */
	std::FILE* output = stderr;
};

/** Complete production error contract selected at App creation. */
struct ErrorContract {
	ErrorPolicy policy{};
	ErrorObserverConfig observer{};
};

namespace detail {

/** Route one occurrence through the active singleton App observer. */
#if FLOW_UI_DEV_MODE
void reportErrorEvent(
	const ErrorEventView& event,
	const char* file = __builtin_FILE(),
	const char* function = __builtin_FUNCTION(),
	std::uint32_t line = __builtin_LINE(),
	std::uint32_t column = __builtin_COLUMN()) noexcept;
#else
void reportErrorEvent(const ErrorEventView& event) noexcept;
#endif

/** Report fatal evidence through the common observer, then prohibit continuation. */
[[noreturn]] inline void terminateForFatalError(FlowUiError error) noexcept {
	reportErrorEvent(ErrorEventView{
		.error = error,
		.kind = ErrorEventKind::Fatal,
		.resolution = ErrorResolution::Terminated,
		.inspection = FatalInspectionCapability::TerminalOnly,
	});
	std::terminate();
}

} // namespace detail

static_assert(noexcept(makeError(ErrorCode::None, ErrorSite::None)));
static_assert(std::is_trivially_copyable_v<ErrorEventView>);
static_assert(std::is_trivially_copyable_v<ErrorSink>);
static_assert(std::is_trivially_copyable_v<ErrorObserverConfig>);
static_assert(noexcept(std::declval<const ErrorSink&>().notify(std::declval<const ErrorEventView&>())));

} // namespace FlowUi
