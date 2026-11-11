#pragma once

#include "FlowUi/Public/errorManagement/ErrorTypes.hpp"
#include <cstdint>
#include <string_view>

namespace FlowUi {

/**
 * Stable identity of the FlowUi operation that detected an error.
 *
 * A site is an operation-level production diagnostic, not a source location.
 * Values are explicitly grouped and numbered because they are part of the
 * public error contract. Published values must never be reused.
 */
enum class ErrorSite : std::uint16_t {
	None = 0x0000, /**< No detecting operation. */

	AppConstruct = 0x0101, /**< App Construct operation; subject kind App. */
	AppInitialize = 0x0102, /**< App Initialize operation; subject kind App. */
	AppPollEvents = 0x0103, /**< App Poll Events operation; subject kind App. */
	AppCreateWindow = 0x0104, /**< App Create Window operation; subject kind Window. */
	AppDestroyWindow = 0x0105, /**< App Destroy Window operation; subject kind Window. */
	AppBeginFrame = 0x0106, /**< App Begin Frame operation; subject kind Window. */
	AppEndFrame = 0x0107, /**< App End Frame operation; subject kind Window. */
	AppDrawFrame = 0x0108, /**< App Draw Frame operation; subject kind Window. */
	AppRecreateWindow = 0x0109, /**< App Recreate Window operation; subject kind Window. */
	AppAcquireFrame = 0x010a, /**< App Acquire Frame operation; subject kind Frame. */
	AppSubmitFrame = 0x010b, /**< App Submit Frame operation; subject kind Frame. */
	AppPresent = 0x010c, /**< App Present operation; subject kind Window. */
	AppShutdown = 0x010d, /**< App Shutdown operation; subject kind App. */
	AppAccessFonts = 0x010e, /**< App Access Fonts operation; subject kind App. */
	AppAccessImages = 0x010f, /**< App Access Images operation; subject kind App. */
	AppAccessThemes = 0x0110, /**< App Access Themes operation; subject kind App. */
	AppAccessElements = 0x0111, /**< App Access Elements operation; subject kind App. */
	AppAccessActions = 0x0112, /**< App Access Actions operation; subject kind App. */
	AppAccessIcons = 0x0113, /**< App Access Icons operation; subject kind App. */
	AppAccessViewports = 0x0114, /**< App Access Viewports operation; subject kind Window. */
	AppAccessUi = 0x0115, /**< App Access Ui operation; subject kind Window. */
	AppAccessWindow = 0x0116, /**< App Access Window operation; subject kind Window. */
	AppAccessDevMonitoring = 0x0117, /**< App Access Dev Monitoring operation; subject kind App. */
	AppRequireQuiescent = 0x0118, /**< App Require Quiescent operation; subject kind Window. */
	AppRequirePlatformThread = 0x0119, /**< App Require Platform Thread operation; subject kind App. */
	AppFrameDispatch = 0x011a, /**< App Frame Dispatch operation; subject kind Window. */

	WindowBackendInitialize = 0x0201, /**< Window Backend Initialize operation; subject kind None. */
	WindowMonitorSelect = 0x0202, /**< Window Monitor Select operation; subject kind None. */
	WindowCreate = 0x0203, /**< Window Create operation; subject kind Window. */
	WindowDestroy = 0x0204, /**< Window Destroy operation; subject kind Window. */
	WindowRequiredExtensions = 0x0205, /**< Window Required Extensions operation; subject kind None. */
	WindowSurfaceCreate = 0x0206, /**< Window Surface Create operation; subject kind Window. */
	WindowPollInput = 0x0207, /**< Window Poll Input operation; subject kind Window. */
	WindowSetTitle = 0x0208, /**< Window Set Title operation; subject kind Window. */
	WindowSetCursor = 0x0209, /**< Window Set Cursor operation; subject kind Window. */
	WindowReadClipboard = 0x020a, /**< Window Read Clipboard operation; subject kind Window. */
	WindowWriteClipboard = 0x020b, /**< Window Write Clipboard operation; subject kind Window. */
	WindowBackendDiagnostic = 0x020c, /**< Window Backend Diagnostic operation; subject kind None. */
	WindowOperation = 0x020d, /**< Runtime native-window operation; subject kind Window. */

	VulkanContextInitialize = 0x0301, /**< Vulkan Context Initialize operation; subject kind App. */
	VulkanInstanceCreate = 0x0302, /**< Vulkan Instance Create operation; subject kind App. */
	VulkanSurfaceQuery = 0x0303, /**< Vulkan Surface Query operation; subject kind Window. */
	VulkanPhysicalDeviceSelect = 0x0304, /**< Vulkan Physical Device Select operation; subject kind App. */
	VulkanLogicalDeviceCreate = 0x0305, /**< Vulkan Logical Device Create operation; subject kind App. */
	VulkanAllocatorCreate = 0x0306, /**< Vulkan Allocator Create operation; subject kind App. */
	VulkanFramesInitialize = 0x0307, /**< Vulkan Frames Initialize operation; subject kind Window. */
	VulkanFrameAcquire = 0x0308, /**< Vulkan Frame Acquire operation; subject kind Frame. */
	VulkanFrameSubmit = 0x0309, /**< Vulkan Frame Submit operation; subject kind Frame. */
	VulkanSwapchainCreate = 0x030a, /**< Vulkan Swapchain Create operation; subject kind Window. */
	VulkanSwapchainAcquire = 0x030b, /**< Vulkan Swapchain Acquire operation; subject kind Window. */
	VulkanSwapchainPresent = 0x030c, /**< Vulkan Swapchain Present operation; subject kind Window. */
	VulkanSwapchainRecreate = 0x030d, /**< Vulkan Swapchain Recreate operation; subject kind Window. */
	VulkanContextShutdown = 0x030e, /**< Vulkan Context Shutdown operation; subject kind App. */
	VulkanDebugDiagnostic = 0x030f, /**< Vulkan Debug Diagnostic operation; subject kind App. */

	RendererInitialize = 0x0401, /**< Renderer Initialize operation; subject kind None. */
	RendererLoadShader = 0x0402, /**< Renderer Load Shader operation; subject kind None. */
	RendererPublishLayout = 0x0403, /**< Renderer Publish Layout operation; subject kind Resource. */
	RendererPublishPipeline = 0x0404, /**< Renderer Publish Pipeline operation; subject kind Resource. */
	RendererCreateWindowResources = 0x0405, /**< Renderer Create Window Resources operation; subject kind Window. */
	RendererConvertCommands = 0x0406, /**< Renderer Convert Commands operation; subject kind Frame. */
	RendererRecordCommands = 0x0407, /**< Renderer Record Commands operation; subject kind Frame. */
	RendererPublishDescriptors = 0x0408, /**< Renderer Publish Descriptors operation; subject kind Window. */
	RendererDraw = 0x0409, /**< Renderer Draw operation; subject kind Frame. */
	RendererDestroyWindowResources = 0x040a, /**< Renderer Destroy Window Resources operation; subject kind Window. */
	RendererShutdown = 0x040b, /**< Renderer Shutdown operation; subject kind None. */

	StorageInitialize = 0x0501, /**< Storage Initialize operation; subject kind None. */
	StorageRegisterWindow = 0x0502, /**< Storage Register Window operation; subject kind Window. */
	StorageUnregisterWindow = 0x0503, /**< Storage Unregister Window operation; subject kind Window. */
	StorageBeginFrame = 0x0504, /**< Storage Begin Frame operation; subject kind Frame. */
	StorageSealFrame = 0x0505, /**< Storage Seal Frame operation; subject kind Frame. */
	StorageCancelFrame = 0x0506, /**< Storage Cancel Frame operation; subject kind Frame. */
	StorageValidateFrame = 0x0507, /**< Storage Validate Frame operation; subject kind Frame. */
	StorageValidateSubmission = 0x0508, /**< Storage Validate Submission operation; subject kind Submission. */
	StorageNoteSubmission = 0x0509, /**< Storage Note Submission operation; subject kind Submission. */
	StorageNoteCompletion = 0x050a, /**< Storage Note Completion operation; subject kind Submission. */
	StorageRetire = 0x050b, /**< Storage Retire operation; subject kind Resource. */
	StorageCollect = 0x050c, /**< Storage Collect operation; subject kind Resource. */
	StorageTrim = 0x050d, /**< Storage Trim operation; subject kind MemoryTarget. */
	StorageSetBudget = 0x050e, /**< Storage Set Budget operation; subject kind MemoryTarget. */
	StorageShutdown = 0x050f, /**< Storage Shutdown operation; subject kind None. */
	StorageCalculateCapacity = 0x0510, /**< Storage Calculate Capacity operation; subject kind MemoryTarget. */
	StorageRequireInitialized = 0x0511, /**< Storage Require Initialized operation; subject kind None. */
	StorageLookupWindow = 0x0512, /**< Storage Lookup Window operation; subject kind Window. */

	ResourceNormalizeKey = 0x0601, /**< Resource Normalize Key operation; subject kind ResourceKey. */
	ResourceInternKey = 0x0602, /**< Resource Intern Key operation; subject kind ResourceKey. */
	ResourceAllocatePersistent = 0x0603, /**< Resource Allocate Persistent operation; subject kind MemoryTarget. */
	ResourceCreateManagerRecord = 0x0604, /**< Resource Create Manager Record operation; subject kind ResourceKey. */
	ResourceLookupManagerRecord = 0x0605, /**< Resource Lookup Manager Record operation; subject kind ResourceKey. */
	ResourceRemoveManagerRecord = 0x0606, /**< Resource Remove Manager Record operation; subject kind ResourceKey. */
	ResourceCreatePersistentRecord = 0x0607, /**< Resource Create Persistent Record operation; subject kind Resource. */
	ResourceLookupPersistentRecord = 0x0608, /**< Resource Lookup Persistent Record operation; subject kind Resource. */
	ResourceRemovePersistentRecord = 0x0609, /**< Resource Remove Persistent Record operation; subject kind Resource. */
	ResourceCreateBlob = 0x060a, /**< Resource Create Blob operation; subject kind Resource. */
	ResourceReadBlob = 0x060b, /**< Resource Read Blob operation; subject kind Resource. */
	ResourceCreateBuffer = 0x060c, /**< Resource Create Buffer operation; subject kind Resource. */
	ResourceWriteBuffer = 0x060d, /**< Resource Write Buffer operation; subject kind Resource. */
	ResourceCreateImage = 0x060e, /**< Resource Create Image operation; subject kind Resource. */
	ResourceCreateImageView = 0x060f, /**< Resource Create Image View operation; subject kind Resource. */
	ResourceAcquireSampler = 0x0610, /**< Resource Acquire Sampler operation; subject kind Resource. */
	ResourcePublishTexture = 0x0611, /**< Resource Publish Texture operation; subject kind ResourceKey. */
	ResourceReplaceTexture = 0x0612, /**< Resource Replace Texture operation; subject kind ResourceKey. */
	ResourceLookupTexture = 0x0613, /**< Resource Lookup Texture operation; subject kind ResourceKey. */
	ResourceRemoveTexture = 0x0614, /**< Resource Remove Texture operation; subject kind ResourceKey. */
	ResourcePrepareTextureBindings = 0x0615, /**< Resource Prepare Texture Bindings operation; subject kind Frame. */
	ResourcePublishTextureBindings = 0x0616, /**< Resource Publish Texture Bindings operation; subject kind Frame. */
	ResourceResolveTexture = 0x0617, /**< Resource Resolve Texture operation; subject kind Resource. */
	ResourceEnqueueUpload = 0x0618, /**< Resource Enqueue Upload operation; subject kind Resource. */
	ResourceFlushUploads = 0x0619, /**< Resource Flush Uploads operation; subject kind Resource. */
	ResourceValidateHandle = 0x061a, /**< Resource Validate Handle operation; subject kind Resource. */
	ResourcePublishNativeObject = 0x061b, /**< Resource Publish Native Object operation; subject kind Resource. */
	ResourceAcquireNativeObject = 0x061c, /**< Resource Acquire Native Object operation; subject kind Resource. */

	UiManagerInitialize = 0x0701, /**< Ui Manager Initialize operation; subject kind None. */
	UiManagerBeginFrame = 0x0702, /**< Ui Manager Begin Frame operation; subject kind Window. */
	UiManagerEndFrame = 0x0703, /**< Ui Manager End Frame operation; subject kind Window. */
	UiManagerRender = 0x0704, /**< Ui Manager Render operation; subject kind Window. */
	UiManagerReset = 0x0705, /**< Ui Manager Reset operation; subject kind Window. */
	UiManagerDefineElement = 0x0706, /**< Ui Manager Define Element operation; subject kind ElementDefinition. */
	UiManagerOpenElement = 0x0707, /**< Ui Manager Open Element operation; subject kind ElementInstance. */
	UiManagerCloseElement = 0x0708, /**< Ui Manager Close Element operation; subject kind ElementInstance. */
	UiManagerInvokeElement = 0x0709, /**< Ui Manager Invoke Element operation; subject kind ElementInstance. */
	UiManagerShutdown = 0x070a, /**< Ui Manager Shutdown operation; subject kind None. */
	UiManagerAccessElements = 0x070b, /**< Ui Manager Access Elements operation; subject kind Window. */
	UiManagerAccessActions = 0x070c, /**< Ui Manager Access Actions operation; subject kind Window. */
	UiManagerAccessTheme = 0x070d, /**< Ui Manager Access Theme operation; subject kind Theme. */

	ElementManagerInitialize = 0x0801, /**< Element Manager Initialize operation; subject kind None. */
	ElementRegisterDefinition = 0x0802, /**< Element Register Definition operation; subject kind ElementDefinition. */
	ElementRegisterWindow = 0x0803, /**< Element Register Window operation; subject kind Window. */
	ElementBeginFrame = 0x0804, /**< Element Begin Frame operation; subject kind Window. */
	ElementResolveState = 0x0805, /**< Element Resolve State operation; subject kind ElementInstance. */
	ElementConstructState = 0x0806, /**< Element Construct State operation; subject kind ElementInstance. */
	ElementEraseState = 0x0807, /**< Element Erase State operation; subject kind ElementInstance. */
	ElementResolveResource = 0x0808, /**< Element Resolve Resource operation; subject kind ElementInstance. */
	ElementConstructResource = 0x0809, /**< Element Construct Resource operation; subject kind ElementInstance. */
	ElementDestroyResource = 0x080a, /**< Element Destroy Resource operation; subject kind ElementInstance. */
	ElementCommitFrame = 0x080b, /**< Element Commit Frame operation; subject kind Window. */
	ElementCancelFrame = 0x080c, /**< Element Cancel Frame operation; subject kind Window. */
	ElementInvoke = 0x080d, /**< Element Invoke operation; subject kind ElementInstance. */
	ElementManagerShutdown = 0x080e, /**< Element Manager Shutdown operation; subject kind None. */

	ActionManagerInitialize = 0x0901, /**< Action Manager Initialize operation; subject kind None. */
	ActionBind = 0x0902, /**< Action Bind operation; subject kind Action. */
	ActionRebind = 0x0903, /**< Action Rebind operation; subject kind Action. */
	ActionUnbind = 0x0904, /**< Action Unbind operation; subject kind Action. */
	ActionSetAvailability = 0x0905, /**< Action Set Availability operation; subject kind Action. */
	ActionPublish = 0x0906, /**< Action Publish operation; subject kind Action. */
	ActionInvoke = 0x0907, /**< Action Invoke operation; subject kind Action. */
	ActionCompleteInvocation = 0x0908, /**< Action Complete Invocation operation; subject kind Action. */
	ActionManagerShutdown = 0x0909, /**< Action Manager Shutdown operation; subject kind None. */

	ThemeManagerInitialize = 0x0a01, /**< Theme Manager Initialize operation; subject kind None. */
	ThemeRegisterType = 0x0a02, /**< Theme Register Type operation; subject kind Theme. */
	ThemeRegisterVariant = 0x0a03, /**< Theme Register Variant operation; subject kind Theme. */
	ThemeSetActiveVariant = 0x0a04, /**< Theme Set Active Variant operation; subject kind Theme. */
	ThemeLookupActiveVariant = 0x0a05, /**< Theme Lookup Active Variant operation; subject kind Theme. */
	ThemeMutate = 0x0a06, /**< Theme Mutate operation; subject kind Theme. */
	ThemeStageMutation = 0x0a07, /**< Theme Stage Mutation operation; subject kind Theme. */
	ThemeCommitMutation = 0x0a08, /**< Theme Commit Mutation operation; subject kind Theme. */
	ThemeRollbackMutation = 0x0a09, /**< Theme Rollback Mutation operation; subject kind Theme. */
	ThemeManagerShutdown = 0x0a0a, /**< Theme Manager Shutdown operation; subject kind None. */

	InputManagerInitialize = 0x0b01, /**< Input Manager Initialize operation; subject kind None. */
	InputRegisterField = 0x0b02, /**< Input Register Field operation; subject kind ElementInstance. */
	InputBeginFrame = 0x0b03, /**< Input Begin Frame operation; subject kind Window. */
	InputEdit = 0x0b04, /**< Input Edit operation; subject kind ElementInstance. */
	InputCopy = 0x0b05, /**< Input Copy operation; subject kind ElementInstance. */
	InputCut = 0x0b06, /**< Input Cut operation; subject kind ElementInstance. */
	InputPaste = 0x0b07, /**< Input Paste operation; subject kind ElementInstance. */
	InputFocus = 0x0b08, /**< Input Focus operation; subject kind ElementInstance. */
	InputBlur = 0x0b09, /**< Input Blur operation; subject kind ElementInstance. */
	InputSubmit = 0x0b0a, /**< Input Submit operation; subject kind ElementInstance. */
	InputQueueEvent = 0x0b0b, /**< Input Queue Event operation; subject kind Window. */
	InputManagerShutdown = 0x0b0c, /**< Input Manager Shutdown operation; subject kind None. */

	PopupManagerInitialize = 0x0c01, /**< Popup Manager Initialize operation; subject kind None. */
	PopupBeginFrame = 0x0c02, /**< Popup Begin Frame operation; subject kind Window. */
	PopupRequest = 0x0c03, /**< Popup Request operation; subject kind Popup. */
	PopupResolveAnchor = 0x0c04, /**< Popup Resolve Anchor operation; subject kind Popup. */
	PopupMeasure = 0x0c05, /**< Popup Measure operation; subject kind Popup. */
	PopupLayout = 0x0c06, /**< Popup Layout operation; subject kind Popup. */
	PopupDismiss = 0x0c07, /**< Popup Dismiss operation; subject kind Popup. */
	PopupEndFrame = 0x0c08, /**< Popup End Frame operation; subject kind Window. */
	PopupManagerShutdown = 0x0c09, /**< Popup Manager Shutdown operation; subject kind None. */

	ShortcutManagerInitialize = 0x0d01, /**< Shortcut Manager Initialize operation; subject kind None. */
	ShortcutRegister = 0x0d02, /**< Shortcut Register operation; subject kind Shortcut. */
	ShortcutUpdate = 0x0d03, /**< Shortcut Update operation; subject kind Shortcut. */
	ShortcutRemove = 0x0d04, /**< Shortcut Remove operation; subject kind Shortcut. */
	ShortcutResolve = 0x0d05, /**< Shortcut Resolve operation; subject kind Shortcut. */
	ShortcutManagerShutdown = 0x0d06, /**< Shortcut Manager Shutdown operation; subject kind None. */

	AssetOpen = 0x0e01, /**< Asset Open operation; subject kind None. */
	AssetRead = 0x0e02, /**< Asset Read operation; subject kind None. */
	AssetValidatePayload = 0x0e03, /**< Asset Validate Payload operation; subject kind None. */
	AssetDecode = 0x0e04, /**< Asset Decode operation; subject kind None. */

	ImageManagerInitialize = 0x0f01, /**< Image Manager Initialize operation; subject kind None. */
	ImageRegister = 0x0f02, /**< Image Register operation; subject kind Image. */
	ImageLoad = 0x0f03, /**< Image Load operation; subject kind Image. */
	ImageDecode = 0x0f04, /**< Image Decode operation; subject kind Image. */
	ImageUpload = 0x0f05, /**< Image Upload operation; subject kind Image. */
	ImagePublish = 0x0f06, /**< Image Publish operation; subject kind Image. */
	ImageLookup = 0x0f07, /**< Image Lookup operation; subject kind Image. */
	ImageRemove = 0x0f08, /**< Image Remove operation; subject kind Image. */
	ImageManagerShutdown = 0x0f09, /**< Image Manager Shutdown operation; subject kind None. */

	FontManagerInitialize = 0x1001, /**< Font Manager Initialize operation; subject kind None. */
	FontRegisterFamily = 0x1002, /**< Font Register Family operation; subject kind Font. */
	FontLoad = 0x1003, /**< Font Load operation; subject kind Font. */
	FontParse = 0x1004, /**< Font Parse operation; subject kind Font. */
	FontBake = 0x1005, /**< Font Bake operation; subject kind Font. */
	FontPublishAtlas = 0x1006, /**< Font Publish Atlas operation; subject kind Font. */
	FontLookupFamily = 0x1007, /**< Font Lookup Family operation; subject kind Font. */
	FontRemoveFamily = 0x1008, /**< Font Remove Family operation; subject kind Font. */
	FontManagerShutdown = 0x1009, /**< Font Manager Shutdown operation; subject kind None. */

	ResourceLocate = 0x1401, /**< Resource Locate operation; subject kind Resource. */
	ResourceReadFile = 0x1402, /**< Resource Read File operation; subject kind Resource. */

	IconManagerInitialize = 0x1101, /**< Icon Manager Initialize operation; subject kind None. */
	IconRegisterSource = 0x1102, /**< Icon Register Source operation; subject kind Icon. */
	IconParseSource = 0x1103, /**< Icon Parse Source operation; subject kind Icon. */
	IconRegisterRaster = 0x1104, /**< Icon Register Raster operation; subject kind Icon. */
	IconRasterize = 0x1105, /**< Icon Rasterize operation; subject kind Icon. */
	IconPublishAtlas = 0x1106, /**< Icon Publish Atlas operation; subject kind Icon. */
	IconLookup = 0x1107, /**< Icon Lookup operation; subject kind Icon. */
	IconManagerShutdown = 0x1108, /**< Icon Manager Shutdown operation; subject kind None. */
	IconRemove = 0x1109, /**< Icon Remove operation; subject kind Icon. */

	ViewportManagerInitialize = 0x1201, /**< Viewport Manager Initialize operation; subject kind None. */
	ViewportCreate = 0x1202, /**< Viewport Create operation; subject kind Viewport. */
	ViewportResize = 0x1203, /**< Viewport Resize operation; subject kind Viewport. */
	ViewportPrepare = 0x1204, /**< Viewport Prepare operation; subject kind Viewport. */
	ViewportRecord = 0x1205, /**< Viewport Record operation; subject kind Viewport. */
	ViewportPublish = 0x1206, /**< Viewport Publish operation; subject kind Viewport. */
	ViewportLookup = 0x1207, /**< Viewport Lookup operation; subject kind Viewport. */
	ViewportDestroy = 0x1208, /**< Viewport Destroy operation; subject kind Viewport. */
	ViewportManagerShutdown = 0x1209, /**< Viewport Manager Shutdown operation; subject kind None. */
	ViewportConfigure = 0x120a, /**< Viewport Configure operation; subject kind Viewport. */

	CallbackAction = 0x1301, /**< Callback Action operation; subject kind Action. */
	CallbackElementConstruct = 0x1302, /**< Callback Element Construct operation; subject kind ElementInstance. */
	CallbackElementDestroy = 0x1303, /**< Callback Element Destroy operation; subject kind ElementInstance. */
	CallbackElementInteraction = 0x1304, /**< Callback Element Interaction operation; subject kind ElementInstance. */
	CallbackThemeMutation = 0x1305, /**< Callback Theme Mutation operation; subject kind Theme. */
	CallbackWindow = 0x1306, /**< Callback Window operation; subject kind Window. */
	CallbackViewportRender = 0x1307, /**< Callback Viewport Render operation; subject kind Viewport. */
	CleanupFrameRollback = 0x1308, /**< Cleanup Frame Rollback operation; subject kind Frame. */
	CleanupResourceRollback = 0x1309, /**< Cleanup Resource Rollback operation; subject kind Resource. */
	CleanupManagerShutdown = 0x130a, /**< Cleanup Manager Shutdown operation; subject kind None. */
	CleanupAppShutdown = 0x130b, /**< Cleanup App Shutdown operation; subject kind App. */

	AgenticConfigure = 0x1601, /**< Agentic Configure operation; subject kind App. */
	AgenticCapture = 0x1602, /**< Agentic Capture operation; subject kind App. */
	AgenticSnapshot = 0x1603, /**< Agentic Snapshot operation; subject kind App. */
	AgenticPublish = 0x1604, /**< Agentic Publish operation; subject kind Resource. */
	AgenticCommand = 0x1605, /**< Agentic Command operation; subject kind App. */
	AgenticTransport = 0x1606, /**< Agentic Transport operation; subject kind App. */
	AgenticScenario = 0x1607, /**< Agentic Scenario operation; subject kind App. */
	AgenticImage = 0x1608, /**< Agentic Image operation; subject kind Window. */
	AgenticCompare = 0x1609, /**< Agentic Compare operation; subject kind Resource. */
};

/** Static catalog data associated with one ErrorSite. */
struct ErrorSiteDescriptor {
	/** Stable operation identity described by this entry. */
	ErrorSite site = ErrorSite::None;
	/** Interpretation of FlowUiError::subject for this operation. */
	ErrorSubjectKind expectedSubject = ErrorSubjectKind::None;
	/** Stable operation name backed by static storage. */
	std::string_view name = "None";
};

/**
 * Return static metadata for site. Unknown values preserve the numeric identity and return None
 * subject kind with the name UnknownErrorSite.
 */
[[nodiscard]] constexpr ErrorSiteDescriptor describeErrorSite(ErrorSite site) noexcept {
#define FLOWUI_ERROR_SITE_CASE(siteName, subjectKind)                     \
	case ErrorSite::siteName:                                             \
		return {                                                          \
			ErrorSite::siteName, ErrorSubjectKind::subjectKind, #siteName \
		}
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
		FLOWUI_ERROR_SITE_CASE(WindowOperation, Window);

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
		FLOWUI_ERROR_SITE_CASE(ResourceLocate, Resource);
		FLOWUI_ERROR_SITE_CASE(ResourceReadFile, Resource);

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
		FLOWUI_ERROR_SITE_CASE(AgenticConfigure, App);
		FLOWUI_ERROR_SITE_CASE(AgenticCapture, App);
		FLOWUI_ERROR_SITE_CASE(AgenticSnapshot, App);
		FLOWUI_ERROR_SITE_CASE(AgenticPublish, Resource);
		FLOWUI_ERROR_SITE_CASE(AgenticCommand, App);
		FLOWUI_ERROR_SITE_CASE(AgenticTransport, App);
		FLOWUI_ERROR_SITE_CASE(AgenticScenario, App);
		FLOWUI_ERROR_SITE_CASE(AgenticImage, Window);
		FLOWUI_ERROR_SITE_CASE(AgenticCompare, Resource);
	}
#undef FLOWUI_ERROR_SITE_CASE
	return {site, ErrorSubjectKind::None, "UnknownErrorSite"};
}

/** Return a static operation name, or UnknownErrorSite for an unrecognized value. */
[[nodiscard]] constexpr std::string_view errorSiteName(ErrorSite site) noexcept {
	return describeErrorSite(site).name;
}

} // namespace FlowUi
