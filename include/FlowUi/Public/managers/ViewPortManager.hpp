#pragma once

#include "FlowUi/Public/BuildConfig.hpp"

#include <array>
#include <cstdint>
#include <functional>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>

#include <clay.h>

#include "FlowUi/Public/BuildConfig.hpp"
#include "FlowUi/Public/TextureRef.hpp"
#include "FlowUi/Public/errorManagement/ErrorCodeTypes.hpp"
#include "FlowUi/Public/errorManagement/ErrorPolicy.hpp"
#include "FlowUi/Public/errorManagement/ErrorSites.hpp"
#include "FlowUi/Public/errorManagement/ErrorValue.hpp"
#include "FlowUi/Public/errorManagement/Exception.hpp"
#include "FlowUi/Public/errorManagement/Result.hpp"
#include "FlowUi/Public/identificationTypes/TextureHandle.hpp"
#include "FlowUi/Public/identificationTypes/WindowId.hpp"
#include "FlowUi/Public/identificationTypes/ResourceKey.hpp"
#include "FlowUi/Public/interop/vulkan/Viewport.hpp"

#if FLOWUI_PUBLIC_VULKAN_INTEROP || defined(FLOWUI_INTERNAL_VIEWPORT_MANAGER)

#include <vulkan/vulkan.h>

struct VulkanContext;

namespace FlowUi {

namespace detail::storage { class IStorageSystem; struct FrameToken; }
namespace detail::manager_storage {
struct ViewportFacadeState; struct ViewportImageResource; struct ViewportFrameCommands;
struct ViewportTargetGeneration; struct ViewportRecord; class ViewportStorageController;
}

#if FLOW_UI_DEV_MODE
namespace devSystems { class DevTimingRecorder; class MemorySampleSink; struct GpuTimingCommandContext; }
#endif

class App;

/** @addtogroup flowui_viewport_manager
 * @{
 */

/**
 * @brief Offscreen render target that can be sampled by the FlowUi renderer.
 *
 * ViewPort represents one application-defined render target managed by
 * ViewPortManager. The viewport owns no public Vulkan resources directly;
 * FlowUi allocates per-frame images, command buffers, image views, and texture
 * slots internally. UI code displays the viewport by using textureRef() or, more
 * commonly, ViewPortManager::getTexture() as Clay image data.
 *
 * A viewport render callback is invoked only for frames where the viewport
 * texture is referenced by the UI. FlowUi begins and ends the secondary command
 * buffer for the callback, creates the dynamic rendering pass around that
 * secondary command buffer, handles image layout transitions, and makes the
 * result available to the UI renderer as a sampled texture.
 *
 * @warning Vulkan handles exposed through ViewPortRenderContext and
 * ViewPortVulkanInterop are owned by FlowUi::App. Callback code may use them to
 * record commands or create compatible resources, but must not destroy or take
 * ownership of App-managed objects.
 */
class ViewPort {
public:
	/** @brief Render callback invoked for a viewport pass. */
	using RenderCallback = std::function<void(const ViewPortRenderContext&)>;

	/**
	 * @brief Return this viewport's key.
	 *
	 * @return Stable key used to create and look up the viewport.
	 *
	 * @code{.cpp}
	 * if (const FlowUi::ViewPort* viewport = app.viewPorts().getViewPort("scene")) {
	 *     std::string key(viewport->getKey());
	 * }
	 * @endcode
	 */
	[[nodiscard]] std::string_view getKey() const noexcept;

	/**
	 * @brief Return whether the viewport has a non-zero render size.
	 *
	 * @retval true The viewport currently has positive width and height.
	 * @retval false The viewport has not been sized yet or is currently empty.
	 *
	 * @code{.cpp}
	 * if (viewport->hasValidSize()) {
	 *     const VkExtent2D extent = viewport->getSize();
	 * }
	 * @endcode
	 */
	[[nodiscard]] bool hasValidSize() const noexcept;

	/**
	 * @brief Return the current render target size.
	 *
	 * ViewPortManager derives this size from the largest UI image area that uses
	 * the viewport texture in the current frame, scaled to framebuffer pixels.
	 *
	 * @return Current viewport extent in pixels. Width or height may be zero
	 * before the viewport has been referenced and sized.
	 *
	 * @code{.cpp}
	 * const VkExtent2D extent = viewport->getSize();
	 * @endcode
	 */
	[[nodiscard]] VkExtent2D getSize() const noexcept;

	/**
	 * @brief Return a texture reference for this viewport.
	 *
	 * The returned TextureRef points at the viewport image for the currently
	 * active frame slot. For UI emission by key, ViewPortManager::getTexture()
	 * is usually more convenient.
	 *
	 * @return TextureRef that can be stored in Clay image data.
	 *
	 * @code{.cpp}
	 * Clay_ImageElementConfig image{};
	 * image.imageData = context.uiManager.imageData(viewport->textureRef());
	 * @endcode
	 */
	[[nodiscard]] TextureRef textureRef() const noexcept;

	/**
	 * @brief Set the viewport render callback.
	 *
	 * The callback records viewport drawing commands into the secondary command
	 * buffer provided by ViewPortRenderContext. FlowUi begins and ends that
	 * command buffer; the callback should not call vkBeginCommandBuffer() or
	 * vkEndCommandBuffer().
	 *
	 * @param callback Callback invoked when the viewport is rendered. Passing an
	 * empty callback disables custom rendering.
	 *
	 * @warning The callback must not destroy App-owned Vulkan handles exposed by
	 * ViewPortRenderContext::vulkan.
	 *
	 * @code{.cpp}
	 * viewport->setRenderCallback([](const FlowUi::ViewPortRenderContext& ctx) {
	 *     VkViewport vkViewport{};
	 *     vkViewport.width = static_cast<float>(ctx.extent.width);
	 *     vkViewport.height = static_cast<float>(ctx.extent.height);
	 *     vkViewport.maxDepth = 1.0f;
	 *     vkCmdSetViewport(ctx.commandBuffer, 0, 1, &vkViewport);
	 *     // Record draw commands here. Do not begin or end ctx.commandBuffer.
	 * });
	 * @endcode
	 */
	void setRenderCallback(RenderCallback callback);

	/**
	 * @brief Set the viewport render callback while retaining typed user data.
	 *
	 * This overload keeps userData alive for as long as the callback remains set
	 * and passes the referenced object to the callback after the render context.
	 *
	 * @tparam T User data object type.
	 * @tparam Fn Callable type invocable with
	 * (const ViewPortRenderContext&, T&).
	 * @param userData Shared payload retained by the callback.
	 * @param callback Callable invoked for viewport rendering.
	 *
	 * @throws FlowUiException with ViewportConfigurationInvalid if userData is null.
	 *
	 * @code{.cpp}
	 * struct SceneRenderer {
	 *     void render(const FlowUi::ViewPortRenderContext& ctx);
	 * };
	 *
	 * auto scene = std::make_shared<SceneRenderer>();
	 * viewport->setRenderCallback(
	 *     scene,
	 *     [](const FlowUi::ViewPortRenderContext& ctx, SceneRenderer& renderer) {
	 *         renderer.render(ctx);
	 *     });
	 * @endcode
	 */
	template <typename T, typename Fn>
	void setRenderCallback(std::shared_ptr<T> userData, Fn&& callback) {
		if (!userData) {
			throw FlowUiException(makeError(ErrorCode::ViewportConfigurationInvalid, ErrorSite::ViewportConfigure));
		}
		using CallbackType = std::decay_t<Fn>;
		setRenderCallback([payload = std::move(userData), typedCallback = CallbackType(std::forward<Fn>(callback))](
			const ViewPortRenderContext& context) mutable {
			typedCallback(context, *payload);
		});
	}

	/**
	 * @brief Clear the viewport render callback.
	 *
	 * After clearing, FlowUi still manages the viewport texture, but no custom
	 * draw commands are recorded for it.
	 *
	 * @code{.cpp}
	 * viewport->clearRenderCallback();
	 * @endcode
	 */
	void clearRenderCallback();

	/**
	 * @brief Return whether a render callback is set.
	 *
	 * @retval true A render callback is installed.
	 * @retval false No render callback is installed.
	 */
	[[nodiscard]] bool hasRenderCallback() const noexcept;

	/**
	 * @brief Set viewport clear color.
	 *
	 * @param r Red channel.
	 * @param g Green channel.
	 * @param b Blue channel.
	 * @param a Alpha channel.
	 *
	 * @code{.cpp}
	 * viewport->setClearColor(0.02f, 0.02f, 0.03f, 1.0f);
	 * @endcode
	 */
	void setClearColor(float r, float g, float b, float a);

	/**
	 * @brief Return viewport clear color.
	 *
	 * @return RGBA clear color used by the viewport render pass.
	 */
	[[nodiscard]] std::array<float, 4> clearColor() const noexcept;

	/**
	 * @brief Set whether the viewport clears every frame.
	 *
	 * When disabled, FlowUi loads the existing image contents after the image has
	 * been initialized. This can be useful for persistent render targets.
	 *
	 * @param enabled true to clear each frame, false to preserve previous
	 * contents when possible.
	 *
	 * @code{.cpp}
	 * viewport->setClearEveryFrame(false);
	 * @endcode
	 */
	void setClearEveryFrame(bool enabled);

	/**
	 * @brief Return whether the viewport clears every frame.
	 *
	 * @retval true The viewport pass clears before rendering each frame.
	 * @retval false The viewport pass loads previous contents after
	 * initialization.
	 */
	[[nodiscard]] bool clearEveryFrame() const noexcept;

private:
	friend class ViewPortManager;
	detail::manager_storage::ViewportFacadeState* state_ = nullptr;
};

/**
 * @brief Creates, tracks, renders, and exposes offscreen viewports.
 *
 * ViewPortManager is owned by App and is available through App::viewPorts()
 * when FLOWUI_PUBLIC_VULKAN_INTEROP is enabled. It creates named offscreen
 * render targets, exposes them as TextureRef values for Clay image elements,
 * sizes them from the UI image area that references them, and invokes
 * ViewPort render callbacks during App's render pass.
 *
 * The manager owns all viewport images, image views, texture slots, command
 * pools, and command buffers. User code configures viewports and records
 * rendering through callbacks; App performs lifecycle, synchronization, image
 * transitions, and presentation integration.
 *
 * @warning Vulkan interop resources returned by getVulkanInterop() are
 * App-managed. Do not destroy or replace those handles from application code.
 *
 * @see @ref md_docs_2tutorials_2viewports__vulkan__interop "Viewports and Vulkan Interop"
 * @see @ref md_docs_2concepts_2managers "Managers"
 */
class ViewPortManager {
public:
#if FLOW_UI_DEV_MODE
	/** @brief Borrowed viewport and immutable-capture metadata; observation does not schedule rendering. */
	struct DevViewportView {
		/** @brief Borrowed window-local viewport registration key. */
		std::string_view key{};
		/** @brief Borrowed completed immutable-copy binding, if available. */
		TextureHandle capture_texture{};
		/** @brief Current source render-target generation. */
		uint64_t generation = 0;
		/** @brief Source frame, target generation, and GPU submission of the completed copy. */
		/** @var uint64_t captured_generation
		 * @brief Render-target generation represented by the immutable capture.
		 */
		/** @var uint64_t captured_submission
		 * @brief GPU submission that produced the immutable capture.
		 */
		/** @var uint64_t captured_frame
		 * @brief Source window-frame identity represented by the completed immutable capture.
		 */
		uint64_t captured_frame = 0, captured_generation = 0, captured_submission = 0;
		/** @brief Current source dimensions in pixels and numeric Vulkan format. */
		/** @var uint32_t height
		 * @brief Current source height in pixels.
		 */
		/** @var uint32_t format
		 * @brief Numeric Vulkan format of the current render target.
		 */
		/** @var uint32_t width
		 * @brief Current source width in pixels.
		 */
		uint32_t width = 0, height = 0, format = 0;
		/** @brief Completed copy dimensions in pixels and numeric Vulkan format. */
		/** @var uint32_t captured_height
		 * @brief Immutable capture height in pixels.
		 */
		/** @var uint32_t captured_format
		 * @brief Numeric Vulkan format of the immutable capture.
		 */
		/** @var uint32_t captured_width
		 * @brief Immutable capture width in pixels.
		 */
		uint32_t captured_width = 0, captured_height = 0, captured_format = 0;
		/** @brief Whether a render callback is installed; clear_every_frame describes its load policy. */
		/** @var bool clear_every_frame
		 * @brief Whether the render target clears before each render pass.
		 */
		/** @var bool has_callback
		 * @brief Whether the viewport has a custom render callback.
		 */
		bool has_callback = false, clear_every_frame = true;
		/** @brief Whether immutable capture is supported, pending, or last failed. */
		/** @var bool capture_pending
		 * @brief Whether an immutable capture has been requested but not completed.
		 */
		/** @var bool capture_failed
		 * @brief Whether the most recent immutable capture failed.
		 */
		/** @var bool capture_supported
		 * @brief Whether the source supports immutable GPU capture.
		 */
		bool capture_supported = false, capture_pending = false, capture_failed = false;
	};
	/** @brief Synchronous observation callback; return false to stop visitation. Arguments are borrowed for the call. */
	using DevViewportVisitor = bool (*)(void*, const DevViewportView&);
	/** Inspect registrations without referencing textures or scheduling render callbacks. */
	void visitDevViewports(void* target, DevViewportVisitor visitor) const;
	/** Opt in to one immutable copy when this viewport naturally renders again. */
	[[nodiscard]] Status request_dev_capture(std::string_view key) noexcept;
	/** Release the observation cache without affecting the viewport or its renderer. */
	void release_dev_capture(std::string_view key) noexcept;
	/** Clone a completed observation's binding without copying or overwriting its image. */
	[[nodiscard]] Result<TextureHandle> retain_dev_capture(std::string_view key, TextureHandle expected_capture) noexcept;
#endif
#if FLOW_UI_DEV_MODE && FLOWUI_DEV_MEMORY_LEVEL >= 2
	/** @brief Append observation-only memory accounting to sink without transferring storage ownership. */
	void appendDevMemorySamples(devSystems::MemorySampleSink& sink) const noexcept;
#endif
	/**
	 * @brief Create a viewport by key.
	 *
	 * The key is used for later lookup and for generating the texture slots that
	 * back each frame in flight. Newly created viewports start with a 1x1 render
	 * target and resize when the viewport texture is referenced by UI image
	 * commands.
	 *
	 * @param key Application-defined viewport key. Must not be empty.
	 * @param createInfo Initial format and clear settings.
	 * @return A result containing true when created or false when the key
	 * already exists. Initialization, key, and resource creation failures are
	 * returned as structured errors.
	 * @throws std::bad_alloc if allocating viewport metadata fails.
	 *
	 * @code{.cpp}
	 * const auto created = app.viewPorts().create("scene", FlowUi::ViewPortCreateInfo{
	 *     .clearColor = {0.02f, 0.02f, 0.03f, 1.0f},
	 * });
	 * if (!created) {
	 *     return; // Report created.error() to the application.
	 * }
	 *
	 * if (FlowUi::ViewPort* scene = app.viewPorts().getViewPort("scene")) {
	 *     scene->setRenderCallback(renderScene);
	 * }
	 * @endcode
	 */
	[[nodiscard]] Result<bool> create(std::string_view key,
									  const ViewPortCreateInfo& createInfo = {});
	/** @brief Create a viewport under a resource key local to this manager's window.
	 * ResourceDomain::Viewport is inferred when unspecified; incompatible explicit domains or scopes are rejected.
	 */
	[[nodiscard]] Result<bool> create(ResourceKey key, const ViewPortCreateInfo& createInfo = {});

	/**
	 * @brief Remove a viewport by key.
	 *
	 * This removes texture slots and retires all per-frame viewport resources
	 * after their exact logical texture uses complete.
	 *
	 * @param key Application-defined viewport key.
	 * @return A result containing true when removed or false when the key is
	 * absent. Invalid manager/key and storage failures are structured errors.
	 *
	 *
	 * @code{.cpp}
	 * (void)app.viewPorts().remove("scene");
	 * @endcode
	 */
	[[nodiscard]] Result<bool> remove(std::string_view key);
	/** @brief Remove a viewport under a resource key local to this manager's window.
	 * ResourceDomain::Viewport is inferred when unspecified; incompatible explicit domains or scopes are rejected.
	 */
	[[nodiscard]] Result<bool> remove(ResourceKey key);

	/**
	 * @brief Return whether a viewport exists.
	 *
	 * @param key Application-defined viewport key.
	 * @retval true A viewport with key exists.
	 * @retval false No viewport with key exists.
	 *
	 * @code{.cpp}
	 * if (!app.viewPorts().contains("scene")) {
	 *     app.viewPorts().create("scene");
	 * }
	 * @endcode
	 */
	[[nodiscard]] bool contains(std::string_view key) const;
	/** @brief Test a viewport resource key local to this manager's window.
	 * ResourceDomain::Viewport is inferred when unspecified; incompatible explicit domains or scopes are rejected.
	 */
	[[nodiscard]] bool contains(ResourceKey key) const;

	/**
	 * @brief Return a mutable viewport by key.
	 *
	 * @param key Application-defined viewport key.
	 * @return Borrowed viewport pointer, or nullptr if missing. The pointer
	 * remains valid until that viewport is removed or the manager is destroyed.
	 *
	 * @code{.cpp}
	 * if (FlowUi::ViewPort* viewport = app.viewPorts().getViewPort("scene")) {
	 *     viewport->setClearEveryFrame(true);
	 * }
	 * @endcode
	 */
	[[nodiscard]] ViewPort* getViewPort(std::string_view key);
	/** @brief Borrow a mutable viewport by window-local resource key; return nullptr when missing.
	 * ResourceDomain::Viewport is inferred when unspecified; incompatible explicit domains or scopes are rejected.
	 */
	[[nodiscard]] ViewPort* getViewPort(ResourceKey key);

	/**
	 * @brief Return an immutable viewport by key.
	 *
	 * @param key Application-defined viewport key.
	 * @return Borrowed viewport pointer, or nullptr if missing. The pointer
	 * remains valid until that viewport is removed or the manager is destroyed.
	 */
	[[nodiscard]] const ViewPort* getViewPort(std::string_view key) const;
	/** @brief Borrow an immutable viewport by window-local resource key; return nullptr when missing.
	 * ResourceDomain::Viewport is inferred when unspecified; incompatible explicit domains or scopes are rejected.
	 */
	[[nodiscard]] const ViewPort* getViewPort(ResourceKey key) const;

	/**
	 * @brief Return the texture reference for a viewport key.
	 *
	 * Use the returned TextureRef as Clay image data to display the viewport in
	 * the UI. Missing keys are reported once and return an invalid TextureHandle;
	 * SkipVisual requests omission, while the fallback policy displays the renderer's
	 * fallback texture.
	 *
	 * @param key Application-defined viewport key.
	 * @return Borrowed reference for the current frame slot, or an unavailable
	 * reference governed by the missing-visual policy. Reacquire each frame.
	 *
	 * @code{.cpp}
	 * Clay_ImageElementConfig image{};
	 * image.imageData = context.uiManager.imageData(
	 *     app.viewPorts().getTexture("scene"));
	 *
	 * CLAY(context.uiManager.toClaySID("scene/preview"), {
	 *     .layout = {
	 *         .sizing = {
	 *             .width = CLAY_SIZING_FIXED(320.0f),
	 *             .height = CLAY_SIZING_FIXED(180.0f),
	 *         },
	 *     },
	 *     .image = image,
	 * }) {}
	 * @endcode
	 */
	[[nodiscard]] TextureRef getTexture(std::string_view key) const;
	/** @brief Resolve a window-local viewport key using the configured missing-visual policy.
	 * ResourceDomain::Viewport is inferred when unspecified; incompatible explicit domains or scopes are rejected.
	 */
	[[nodiscard]] TextureRef getTexture(ResourceKey key) const;

	/**
	 * @brief Return Vulkan interop handles for custom viewport rendering.
	 *
	 * The returned handles describe the App-owned Vulkan instance, device,
	 * allocator, graphics queue, and frame count used by FlowUi. They are useful
	 * for creating resources compatible with viewport render callbacks.
	 *
	 * @return Shared Vulkan interop handle bundle.
	 *
	 * @throws FlowUiException with ObjectNotInitialized if the manager is unavailable.
	 *
	 * @warning The returned Vulkan handles are owned by FlowUi::App. Application
	 * code must not destroy them.
	 *
	 * @code{.cpp}
	 * const FlowUi::ViewPortVulkanInterop& vk = app.viewPorts().getVulkanInterop();
	 * VkDevice device = vk.device;
	 * uint32_t graphicsFamily = vk.graphicsQueueFamily;
	 * @endcode
	 *
	 * @see @ref md_docs_2tutorials_2viewports__vulkan__interop "Viewports and Vulkan Interop"
	 */
	[[nodiscard]] const ViewPortVulkanInterop& getVulkanInterop() const;

private:
	friend class App;

	void init(
		detail::storage::IStorageSystem& storage,
		VulkanContext& vk,
		WindowId window,
		uint32_t framesInFlight,
		MissingVisualPolicy missingPolicy);
	void onFrameStart(VulkanContext& vk, uint32_t frameIndex);
#if FLOW_UI_DEV_MODE
	void prepare_dev_captures(const detail::storage::FrameToken& frame) noexcept;
	void mark_dev_captures_submitted(uint64_t serial) noexcept;
	void collect_dev_captures() noexcept;
#endif
	void prepareFrameTargets(
		Clay_RenderCommandArray& renderCommands,
		float uiToFramebufferScaleX,
		float uiToFramebufferScaleY);
	void remapRenderCommandsForFrame(Clay_RenderCommandArray& renderCommands, uint32_t frameIndex);
#if FLOW_UI_AGENTIC_WORKFLOW
	// Supply the authoritative consumer frame before recording any viewport passes.
	void set_agentic_frame_context(uint64_t app_tick, uint64_t window_frame) noexcept;
	// Commit content identities only after their owning command buffer was submitted.
	void mark_agentic_submitted(uint64_t submission_serial) noexcept;
#endif
	void recordFramePasses(
		VulkanContext& vk,
		VkCommandBuffer primaryCommandBuffer,
		uint32_t frameIndex
#if FLOW_UI_DEV_MODE
		,
		devSystems::DevTimingRecorder* timingRecorder = nullptr,
		devSystems::GpuTimingCommandContext* gpuTiming = nullptr
#endif
		);
	// The owning AppWindow must be drained before this teardown is called.
	void destroyDrained(VulkanContext& vk);

	void resetFrameTracking();
	bool resizeRequired() const;
	detail::storage::IStorageSystem* storage_ = nullptr;
	MissingVisualPolicy missingPolicy_ = MissingVisualPolicy::UseFallbackTexture;
	WindowId windowId_ = InvalidWindowId;
	uint64_t controllerHandle_ = 0;
	detail::manager_storage::ViewportStorageController* controller_ = nullptr;
#if FLOW_UI_AGENTIC_WORKFLOW
	uint64_t agentic_app_tick_ = 0;
	uint64_t agentic_window_frame_ = 0;
#endif
};

/** @} */

} // namespace FlowUi

#endif
