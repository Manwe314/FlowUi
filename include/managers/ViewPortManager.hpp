#pragma once

#include "FlowUi/BuildConfig.hpp"

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

#include "FlowUi/BuildConfig.hpp"
#include "FlowUi/PublicStructs.hpp"
#include "FlowUi/ResourceKey.hpp"
#include "managers/structs/ViewPortManagerStructs.hpp"

#if FLOWUI_PUBLIC_VULKAN_INTEROP || defined(FLOWUI_INTERNAL_VIEWPORT_MANAGER)

#include <vulkan/vulkan.h>

struct VulkanContext;
struct VmaAllocation_T;

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
	std::string_view getKey() const;

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
	bool hasValidSize() const;

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
	VkExtent2D getSize() const;

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
	TextureRef textureRef() const;

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
	 * @throws std::runtime_error if userData is null.
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
			throw std::runtime_error("ViewPort::setRenderCallback requires a non-null shared_ptr payload.");
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
	bool hasRenderCallback() const;

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
	std::array<float, 4> clearColor() const;

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
	bool clearEveryFrame() const;

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
#if FLOW_UI_DEV_MODE && FLOWUI_DEV_MEMORY_LEVEL >= 2
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
	 * @retval true A new viewport was created.
	 * @retval false A viewport with key already exists.
	 *
	 * @throws std::runtime_error if the manager is not initialized, the texture
	 * registry is unavailable, key is empty, or Vulkan resource creation fails.
	 *
	 * @code{.cpp}
	 * app.viewPorts().create("scene", FlowUi::ViewPortCreateInfo{
	 *     .clearColor = {0.02f, 0.02f, 0.03f, 1.0f},
	 * });
	 *
	 * if (FlowUi::ViewPort* scene = app.viewPorts().getViewPort("scene")) {
	 *     scene->setRenderCallback(renderScene);
	 * }
	 * @endcode
	 */
	bool create(std::string_view key, const ViewPortCreateInfo& createInfo = {});
	bool create(ResourceKey key, const ViewPortCreateInfo& createInfo = {});

	/**
	 * @brief Remove a viewport by key.
	 *
	 * This removes texture slots and retires all per-frame viewport resources
	 * after their exact logical texture uses complete.
	 *
	 * @param key Application-defined viewport key.
	 * @retval true A viewport with key existed and was removed.
	 * @retval false No viewport with key exists.
	 *
	 * @throws std::runtime_error if the manager or its storage scope is not
	 * initialized.
	 *
	 * @code{.cpp}
	 * (void)app.viewPorts().remove("scene");
	 * @endcode
	 */
	bool remove(std::string_view key);
	bool remove(ResourceKey key);

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
	bool contains(std::string_view key) const;
	bool contains(ResourceKey key) const;

	/**
	 * @brief Return a mutable viewport by key.
	 *
	 * @param key Application-defined viewport key.
	 * @return Pointer to the viewport, or nullptr if missing.
	 *
	 * @code{.cpp}
	 * if (FlowUi::ViewPort* viewport = app.viewPorts().getViewPort("scene")) {
	 *     viewport->setClearEveryFrame(true);
	 * }
	 * @endcode
	 */
	ViewPort* getViewPort(std::string_view key);
	ViewPort* getViewPort(ResourceKey key);

	/**
	 * @brief Return an immutable viewport by key.
	 *
	 * @param key Application-defined viewport key.
	 * @return Pointer to the viewport, or nullptr if missing.
	 */
	const ViewPort* getViewPort(std::string_view key) const;
	const ViewPort* getViewPort(ResourceKey key) const;

	/**
	 * @brief Return the texture reference for a viewport key.
	 *
	 * Use the returned TextureRef as Clay image data to display the viewport in
	 * the UI. If the key is missing, FlowUi logs a warning once for that key and
	 * returns a fallback TextureRef with id 0.
	 *
	 * @param key Application-defined viewport key.
	 * @return TextureRef for the current frame's viewport image, or fallback id
	 * 0 when the viewport is missing or unavailable.
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
	TextureRef getTexture(std::string_view key) const;
	TextureRef getTexture(ResourceKey key) const;

	/**
	 * @brief Return Vulkan interop handles for custom viewport rendering.
	 *
	 * The returned handles describe the App-owned Vulkan instance, device,
	 * allocator, graphics queue, and frame count used by FlowUi. They are useful
	 * for creating resources compatible with viewport render callbacks.
	 *
	 * @return Shared Vulkan interop handle bundle.
	 *
	 * @throws std::runtime_error if the manager is not initialized.
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
	const ViewPortVulkanInterop& getVulkanInterop() const;

private:
	friend class App;

	void init(detail::storage::IStorageSystem& storage, VulkanContext& vk, WindowId window, uint32_t framesInFlight);
	void onFrameStart(VulkanContext& vk, uint32_t frameIndex);
	void prepareFrameTargets(
		const Clay_RenderCommandArray& renderCommands,
		float uiToFramebufferScaleX,
		float uiToFramebufferScaleY);
	void remapRenderCommandsForFrame(Clay_RenderCommandArray& renderCommands, uint32_t frameIndex);
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
	WindowId windowId_ = InvalidWindowId;
	uint64_t controllerHandle_ = 0;
	detail::manager_storage::ViewportStorageController* controller_ = nullptr;
};

/** @} */

} // namespace FlowUi

#endif
