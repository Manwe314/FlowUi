#pragma once

#include "FlowUi/Private/ApiBoundary.hpp"

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
	 * @brief Borrow this viewport's registration key.
	 *
	 * The view is invalid after removal or manager/window destruction.
	 *
	 * @return Returns a view of the manager-owned key, or an empty view when detached.
	 */
	[[nodiscard]] std::string_view getKey() const noexcept;

	/**
	 * @brief Test whether the current render target has nonzero width and height.
	 *
	 * A newly created viewport normally starts at 1x1; later sizing follows UI image usage.
	 *
	 * @return Returns false when detached or unsized.
	 */
	[[nodiscard]] bool hasValidSize() const noexcept;

	/**
	 * @brief Read the current viewport render extent in framebuffer pixels.
	 *
	 * The largest UI image area referencing this viewport drives resizing, with framebuffer scale and configured hysteresis.
	 *
	 * @return Returns VkExtent2D; detached/unsized state may contain zero dimensions.
	 */
	[[nodiscard]] VkExtent2D getSize() const noexcept;

	/**
	 * @brief Read this viewport's current frame-slot TextureRef.
	 *
	 * Reacquire per frame and after resizing; ViewPortManager::getTexture() is normally more convenient for keyed UI emission.
	 *
	 * @return Returns an unavailable TextureRef when detached.
	 */
	[[nodiscard]] TextureRef textureRef() const noexcept;

	/**
	 * @brief Prepare and publish an owned viewport render callback.
	 *
	 * FlowUi begins/ends the provided secondary command buffer and owns the surrounding pass/transitions. Record commands only; do not begin/end/submit it or destroy App-owned handles. The generic overload constructs an owned std::function before publication. The shared_ptr overload retains its payload until the callback is replaced/cleared and rejects null payloads.
	 *
	 * @return Status reports detached/phase/configuration validation failures without replacing the previous callback. An empty RenderCallback disables rendering.
	 * @throws Application callable copy/move/allocation and incidental synchronization exceptions propagate before publication; the previous configuration remains unchanged.
	 *
	 * @code{.cpp}
	 * const FlowUi::ViewPort::RenderCallback callback = [](const FlowUi::ViewPortRenderContext& context) {
	 *     VkViewport dynamicViewport{};
	 *     dynamicViewport.width = static_cast<float>(context.extent.width);
	 *     dynamicViewport.height = static_cast<float>(context.extent.height);
	 *     dynamicViewport.maxDepth = 1.0f;
	 *     vkCmdSetViewport(context.commandBuffer, 0, 1, &dynamicViewport);
	 * };
	 * auto result = viewport.setRenderCallback(callback);
	 * if (!result) {
	 *     throw FlowUi::FlowUiException(result.error());
	 * }
	 * @endcode
	 */
	[[nodiscard]] Status setRenderCallback(const RenderCallback& callback);
	/**
	 * @brief Prepare and publish an owned viewport render callback.
	 *
	 * FlowUi begins/ends the provided secondary command buffer and owns the surrounding pass/transitions. Record commands only; do not begin/end/submit it or destroy App-owned handles. The generic overload constructs an owned std::function before publication. The shared_ptr overload retains its payload until the callback is replaced/cleared and rejects null payloads.
	 *
	 * @return Status reports detached/phase/configuration validation failures without replacing the previous callback. An empty RenderCallback disables rendering.
	 * @throws Application callable copy/move/allocation and incidental synchronization exceptions propagate before publication; the previous configuration remains unchanged.
	 *
	 * @code{.cpp}
	 * auto result = viewport.setRenderCallback([](const FlowUi::ViewPortRenderContext& context) {
	 *     VkViewport dynamicViewport{};
	 *     dynamicViewport.width = static_cast<float>(context.extent.width);
	 *     dynamicViewport.height = static_cast<float>(context.extent.height);
	 *     dynamicViewport.maxDepth = 1.0f;
	 *     vkCmdSetViewport(context.commandBuffer, 0, 1, &dynamicViewport);
	 * });
	 * if (!result) {
	 *     throw FlowUi::FlowUiException(result.error());
	 * }
	 * @endcode
	 */
	template <typename Callback>
		requires (!std::is_same_v<std::remove_cvref_t<Callback>, RenderCallback>)
	[[nodiscard]] Status setRenderCallback(Callback&& callback) {
		return setRenderCallback(RenderCallback(std::forward<Callback>(callback)));
	}


	/**
	 * @brief Prepare and publish an owned viewport render callback.
	 *
	 * FlowUi begins/ends the provided secondary command buffer and owns the surrounding pass/transitions. Record commands only; do not begin/end/submit it or destroy App-owned handles. The generic overload constructs an owned std::function before publication. The shared_ptr overload retains its payload until the callback is replaced/cleared and rejects null payloads.
	 *
	 * @return Status reports detached/phase/configuration validation failures without replacing the previous callback. An empty RenderCallback disables rendering.
	 * @throws Application callable copy/move/allocation and incidental synchronization exceptions propagate before publication; the previous configuration remains unchanged.
	 *
	 * @code{.cpp}
	 * auto blendConstants = std::make_shared<std::array<float, 4>>(
	 *     std::array<float, 4>{1.0f, 1.0f, 1.0f, 1.0f});
	 * auto result = viewport.setRenderCallback(std::move(blendConstants), [](const FlowUi::ViewPortRenderContext& context, std::array<float, 4>& constants) {
	 *     vkCmdSetBlendConstants(context.commandBuffer, constants.data());
	 * });
	 * if (!result) {
	 *     throw FlowUi::FlowUiException(result.error());
	 * }
	 * @endcode
	 */
	template <typename T, typename Fn>
	[[nodiscard]] Status setRenderCallback(std::shared_ptr<T> userData, Fn&& callback) {
		if (!userData) {
			return unexpectedError(makeError(ErrorCode::ViewportConfigurationInvalid, ErrorSite::ViewportConfigure));
		}
		using CallbackType = std::decay_t<Fn>;
		return setRenderCallback([payload = std::move(userData), typedCallback = CallbackType(std::forward<Fn>(callback))](
			const ViewPortRenderContext& context) mutable {
			typedCallback(context, *payload);
		});
	}

	/**
	 * @brief Clear the installed viewport render callback.
	 *
	 * The texture remains managed and may still be cleared; no custom drawing callback is invoked.
	 *
	 * @return Status reports detached/phase validation failure without replacing the existing callback.
	 * @throws Application callable copy/move/allocation and incidental synchronization exceptions propagate before publication; the previous configuration remains unchanged.
	 *
	 * @code{.cpp}
	 * auto result = viewport.clearRenderCallback();
	 * if (!result) {
	 *     throw FlowUi::FlowUiException(result.error());
	 * }
	 * @endcode
	 */
	[[nodiscard]] Status clearRenderCallback();

	/**
	 * @brief Test whether this viewport has an installed callback.
	 *
	 * This is a direct noexcept observation.
	 *
	 * @return Returns false when detached or no callback is installed.
	 */
	[[nodiscard]] bool hasRenderCallback() const noexcept;

	/**
	 * @brief Publish the RGBA viewport clear color.
	 *
	 * The four float channels are stored as provided.
	 *
	 * @return Status reports detached/phase validation failures without changing the color.
	 * @throws Application callable copy/move/allocation and incidental synchronization exceptions propagate before publication; the previous configuration remains unchanged.
	 *
	 * @code{.cpp}
	 * auto result = viewport.setClearColor(0.02f, 0.02f, 0.03f, 1.0f);
	 * if (!result) {
	 *     throw FlowUi::FlowUiException(result.error());
	 * }
	 * @endcode
	 */
	[[nodiscard]] Status setClearColor(float r, float g, float b, float a);

	/**
	 * @brief Read the viewport's RGBA clear color.
	 *
	 * This is a direct noexcept observation.
	 *
	 * @return Returns a copied `std::array<float, 4>`; detached state returns zeros.
	 */
	[[nodiscard]] std::array<float, 4> clearColor() const noexcept;

	/**
	 * @brief Choose whether every pass clears the viewport.
	 *
	 * false preserves image contents where available after the image's required initialization clear.
	 *
	 * @return Status reports detached/phase validation failures without changing the policy.
	 * @throws Application callable copy/move/allocation and incidental synchronization exceptions propagate before publication; the previous configuration remains unchanged.
	 *
	 * @code{.cpp}
	 * auto result = viewport.setClearEveryFrame(false);
	 * if (!result) {
	 *     throw FlowUi::FlowUiException(result.error());
	 * }
	 * @endcode
	 */
	[[nodiscard]] Status setClearEveryFrame(bool enabled);

	/**
	 * @brief Read the viewport clear policy.
	 *
	 * false means previous initialized contents can be loaded.
	 *
	 * @return Returns the stored policy; detached state returns true.
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
	/**
	 * @brief Observe development catalogue entries without invoking application operations.
	 *
	 * Development-only synchronous observation under FLOW_UI_DEV_MODE. The example counts sized viewport registrations; completed is the separate successful bool payload. Views and their text/pointers/handles are borrowed; copy needed values during the callback, do not mutate the catalogue while visiting, and do not retain callback arguments. Visitor exceptions are converted to FlowUiError. Earlier callback effects, including the counter, are not rolled back on failure. This does not schedule rendering or capture; capture_texture is an observation-cache binding and must not be retained as ownership.
	 *
	 * @return Success contains true after visiting the whole catalogue (including an empty catalogue), or false when a visitor stops or for a detached manager or null visitor. A failed Result is an operational/callback error, not an incomplete-visit bool.
	 *
	 * @code{.cpp}
	 * #if FLOW_UI_DEV_MODE && FLOWUI_PUBLIC_VULKAN_INTEROP
	 * std::size_t count = 0;
	 * auto result = manager.visitDevViewports(&count,
	 *     [](void* target, const FlowUi::ViewPortManager::DevViewportView& viewport) {
	 *         auto& observedCount = *static_cast<std::size_t*>(target);
	 *         if (viewport.width != 0 && viewport.height != 0) {
	 *             ++observedCount;
	 *         }
	 *         return true;
	 *     });
	 * if (!result) {
	 *     throw FlowUi::FlowUiException(result.error());
	 * }
	 * const bool completed = *result;
	 * #endif
	 * @endcode
	 */
	[[nodiscard]] Result<bool> visitDevViewports(void* target, DevViewportVisitor visitor) const noexcept;
	/**
	 * @brief Request one immutable development copy on a later natural viewport render.
	 *
	 * The example expects scene to exist. Successful submission only schedules a request: it does not force rendering or mean the GPU copy has completed. Poll visitDevViewports() for capture_pending/capture_failed and a nonempty capture_texture. Capture is format checked and bounded to 16 active observation caches. No exception escapes.
	 *
	 * @return Success when requested; ObjectNotInitialized, ViewportNotFound, unsupported-format ViewportConfigurationInvalid, capacity, storage, or allocation failure otherwise.
	 *
	 * @code{.cpp}
	 * #if FLOW_UI_DEV_MODE && FLOWUI_PUBLIC_VULKAN_INTEROP
	 * auto status = viewports.request_dev_capture("scene");
	 * if (!status) {
	 *     throw FlowUi::FlowUiException(status.error());
	 * }
	 * #endif
	 * @endcode
	 */
	[[nodiscard]] Status request_dev_capture(std::string_view key) noexcept;
	/** Release the observation cache without affecting the viewport or its renderer. */
	void release_dev_capture(std::string_view key) noexcept;
	/**
	 * @brief Create an independent texture binding for an exact completed development capture.
	 *
	 * Pass the exact capture_texture observed for the same key by visitDevViewports(); incomplete or replaced captures are ResourceHandleInvalid. The image is shared without another copy. This retained logical binding must later be released through the owning App::release_dev_viewport_capture_texture(); release_dev_capture(key) only releases the observation cache. Keep the owning App alive. No exception escapes.
	 *
	 * @return A retained TextureHandle, or initialization, missing-viewport, stale/incomplete-handle, storage, or allocation failure.
	 *
	 * @code{.cpp}
	 * #if FLOW_UI_DEV_MODE && FLOWUI_PUBLIC_VULKAN_INTEROP
	 * auto result = viewports.retain_dev_capture("scene", observedCapture);
	 * if (!result) {
	 *     throw FlowUi::FlowUiException(result.error());
	 * }
	 * const FlowUi::TextureHandle retained = *result;
	 * #endif
	 * @endcode
	 */
	[[nodiscard]] Result<TextureHandle> retain_dev_capture(std::string_view key, TextureHandle expected_capture) noexcept;
#endif
#if FLOW_UI_DEV_MODE && FLOWUI_DEV_MEMORY_LEVEL >= 2
	/** @brief Append observation-only memory accounting to sink without transferring storage ownership. */
	void appendDevMemorySamples(devSystems::MemorySampleSink& sink) const noexcept;
#endif
	/**
	 * @brief Create a window-local viewport and its initial 1x1 render targets.
	 *
	 * The viewport resizes from the largest referenced UI image area, scaled to framebuffer pixels. colorFormat == VK_FORMAT_UNDEFINED selects VK_FORMAT_R8G8B8A8_UNORM; createInfo defaults to {}.
	 *
	 * @return Success currently contains true. A duplicate key returns ViewportKeyCollision, not a successful false value. Invalid configuration/manager/key and allocation/GPU resource failures are errors.
	 * Recoverable initialization, validation, allocation, and storage failures are returned as FlowUiError; no exception escapes this noexcept operation.
	 *
	 * @code{.cpp}
	 * auto result = manager.create("scene", FlowUi::ViewPortCreateInfo{});
	 * if (!result) {
	 *     throw FlowUi::FlowUiException(result.error());
	 * }
	 * const auto created = *result;
	 * @endcode
	 */
	[[nodiscard]] Result<bool> create(std::string_view key,
									  const ViewPortCreateInfo& createInfo = {}) noexcept;
	/**
	 * @brief Create a window-local viewport and its initial 1x1 render targets.
	 *
	 * The viewport resizes from the largest referenced UI image area, scaled to framebuffer pixels. colorFormat == VK_FORMAT_UNDEFINED selects VK_FORMAT_R8G8B8A8_UNORM; createInfo defaults to {}. ResourceKey overloads infer the manager's resource domain when unspecified and require this manager's window-local scope; incompatible explicit domains/scopes are errors.
	 *
	 * @return Success currently contains true. A duplicate key returns ViewportKeyCollision, not a successful false value. Invalid configuration/manager/key and allocation/GPU resource failures are errors.
	 * Recoverable initialization, validation, allocation, and storage failures are returned as FlowUiError; no exception escapes this noexcept operation.
	 *
	 * @code{.cpp}
	 * const FlowUi::ResourceKey key{.name = "scene"};
	 * auto result = manager.create(key, FlowUi::ViewPortCreateInfo{});
	 * if (!result) {
	 *     throw FlowUi::FlowUiException(result.error());
	 * }
	 * const auto created = *result;
	 * @endcode
	 */
	[[nodiscard]] Result<bool> create(ResourceKey key, const ViewPortCreateInfo& createInfo = {}) noexcept;

	/**
	 * @brief Remove a viewport and retire its per-frame targets.
	 *
	 * Borrowed ViewPort pointers and their key views become invalid immediately; GPU targets retire after their recorded texture uses complete.
	 *
	 * @return Success contains true if removed or false if absent.
	 * Recoverable initialization, validation, allocation, and storage failures are returned as FlowUiError; no exception escapes this noexcept operation.
	 *
	 * @code{.cpp}
	 * auto result = manager.remove("scene");
	 * if (!result) {
	 *     throw FlowUi::FlowUiException(result.error());
	 * }
	 * const auto removed = *result;
	 * @endcode
	 */
	[[nodiscard]] Result<bool> remove(std::string_view key) noexcept;
	/**
	 * @brief Remove a viewport and retire its per-frame targets.
	 *
	 * Borrowed ViewPort pointers and their key views become invalid immediately; GPU targets retire after their recorded texture uses complete. ResourceKey overloads infer the manager's resource domain when unspecified and require this manager's window-local scope; incompatible explicit domains/scopes are errors.
	 *
	 * @return Success contains true if removed or false if absent.
	 * Recoverable initialization, validation, allocation, and storage failures are returned as FlowUiError; no exception escapes this noexcept operation.
	 *
	 * @code{.cpp}
	 * const FlowUi::ResourceKey key{.name = "scene"};
	 * auto result = manager.remove(key);
	 * if (!result) {
	 *     throw FlowUi::FlowUiException(result.error());
	 * }
	 * const auto removed = *result;
	 * @endcode
	 */
	[[nodiscard]] Result<bool> remove(ResourceKey key) noexcept;

	/**
	 * @brief Test whether a viewport is registered in this window.
	 *
	 * Key normalization/initialization errors are distinct from an absent key.
	 *
	 * @return Success contains true if present or false if absent.
	 * Recoverable initialization, validation, allocation, and storage failures are returned as FlowUiError; no exception escapes this noexcept operation.
	 *
	 * @code{.cpp}
	 * auto result = manager.contains("scene");
	 * if (!result) {
	 *     throw FlowUi::FlowUiException(result.error());
	 * }
	 * const auto registered = *result;
	 * @endcode
	 */
	[[nodiscard]] Result<bool> contains(std::string_view key) const noexcept;
	/**
	 * @brief Test whether a viewport is registered in this window.
	 *
	 * Key normalization/initialization errors are distinct from an absent key. ResourceKey overloads infer the manager's resource domain when unspecified and require this manager's window-local scope; incompatible explicit domains/scopes are errors.
	 *
	 * @return Success contains true if present or false if absent.
	 * Recoverable initialization, validation, allocation, and storage failures are returned as FlowUiError; no exception escapes this noexcept operation.
	 *
	 * @code{.cpp}
	 * const FlowUi::ResourceKey key{.name = "scene"};
	 * auto result = manager.contains(key);
	 * if (!result) {
	 *     throw FlowUi::FlowUiException(result.error());
	 * }
	 * const auto registered = *result;
	 * @endcode
	 */
	[[nodiscard]] Result<bool> contains(ResourceKey key) const noexcept;

	/**
	 * @brief Borrow a viewport facade by key.
	 *
	 * The mutable and const manager overloads return ViewPort* and const ViewPort* respectively. The pointer lasts until that viewport is removed or the manager/window is destroyed; do not retain it across removal/recreation.
	 *
	 * @return Success contains a borrowed pointer, which is nullptr for a missing key. Check both the Result and its contained pointer before dereferencing.
	 * Recoverable initialization, validation, allocation, and storage failures are returned as FlowUiError; no exception escapes this noexcept operation.
	 *
	 * @code{.cpp}
	 * auto result = manager.getViewPort("scene");
	 * if (!result) {
	 *     throw FlowUi::FlowUiException(result.error());
	 * }
	 * FlowUi::ViewPort* viewport = *result;
	 * if (viewport != nullptr) {
	 *     const bool sized = viewport->hasValidSize();
	 * }
	 * @endcode
	 */
	[[nodiscard]] Result<ViewPort*> getViewPort(std::string_view key) noexcept;
	/**
	 * @brief Borrow a viewport facade by key.
	 *
	 * The mutable and const manager overloads return ViewPort* and const ViewPort* respectively. The pointer lasts until that viewport is removed or the manager/window is destroyed; do not retain it across removal/recreation. ResourceKey overloads infer the manager's resource domain when unspecified and require this manager's window-local scope; incompatible explicit domains/scopes are errors.
	 *
	 * @return Success contains a borrowed pointer, which is nullptr for a missing key. Check both the Result and its contained pointer before dereferencing.
	 * Recoverable initialization, validation, allocation, and storage failures are returned as FlowUiError; no exception escapes this noexcept operation.
	 *
	 * @code{.cpp}
	 * const FlowUi::ResourceKey key{.name = "scene"};
	 * auto result = manager.getViewPort(key);
	 * if (!result) {
	 *     throw FlowUi::FlowUiException(result.error());
	 * }
	 * FlowUi::ViewPort* viewport = *result;
	 * if (viewport != nullptr) {
	 *     const bool sized = viewport->hasValidSize();
	 * }
	 * @endcode
	 */
	[[nodiscard]] Result<ViewPort*> getViewPort(ResourceKey key) noexcept;

	/**
	 * @brief Borrow a viewport facade by key.
	 *
	 * The mutable and const manager overloads return ViewPort* and const ViewPort* respectively. The pointer lasts until that viewport is removed or the manager/window is destroyed; do not retain it across removal/recreation.
	 *
	 * @return Success contains a borrowed pointer, which is nullptr for a missing key. Check both the Result and its contained pointer before dereferencing.
	 * Recoverable initialization, validation, allocation, and storage failures are returned as FlowUiError; no exception escapes this noexcept operation.
	 *
	 * @code{.cpp}
	 * auto result = manager.getViewPort("scene");
	 * if (!result) {
	 *     throw FlowUi::FlowUiException(result.error());
	 * }
	 * const FlowUi::ViewPort* viewport = *result;
	 * if (viewport != nullptr) {
	 *     const bool sized = viewport->hasValidSize();
	 * }
	 * @endcode
	 */
	[[nodiscard]] Result<const ViewPort*> getViewPort(std::string_view key) const noexcept;
	/**
	 * @brief Borrow a viewport facade by key.
	 *
	 * The mutable and const manager overloads return ViewPort* and const ViewPort* respectively. The pointer lasts until that viewport is removed or the manager/window is destroyed; do not retain it across removal/recreation. ResourceKey overloads infer the manager's resource domain when unspecified and require this manager's window-local scope; incompatible explicit domains/scopes are errors.
	 *
	 * @return Success contains a borrowed pointer, which is nullptr for a missing key. Check both the Result and its contained pointer before dereferencing.
	 * Recoverable initialization, validation, allocation, and storage failures are returned as FlowUiError; no exception escapes this noexcept operation.
	 *
	 * @code{.cpp}
	 * const FlowUi::ResourceKey key{.name = "scene"};
	 * auto result = manager.getViewPort(key);
	 * if (!result) {
	 *     throw FlowUi::FlowUiException(result.error());
	 * }
	 * const FlowUi::ViewPort* viewport = *result;
	 * if (viewport != nullptr) {
	 *     const bool sized = viewport->hasValidSize();
	 * }
	 * @endcode
	 */
	[[nodiscard]] Result<const ViewPort*> getViewPort(ResourceKey key) const noexcept;

	/**
	 * @brief Acquire the viewport texture reference for the current frame slot.
	 *
	 * Reacquire each frame because frame-slot images change. The TextureRef conveys a generation-checked borrowed resource identity, not ownership.
	 *
	 * @return Missing keys are successful unavailable references, with an invalid handle and a once-per-key diagnostic. SkipVisual omits the visual; the fallback policy uses the renderer fallback.
	 * Recoverable initialization, validation, allocation, and storage failures are returned as FlowUiError; no exception escapes this noexcept operation.
	 *
	 * @code{.cpp}
	 * auto result = manager.getTexture("scene");
	 * if (!result) {
	 *     throw FlowUi::FlowUiException(result.error());
	 * }
	 * const auto texture = *result;
	 * @endcode
	 */
	[[nodiscard]] Result<TextureRef> getTexture(std::string_view key) const noexcept;
	/**
	 * @brief Acquire the viewport texture reference for the current frame slot.
	 *
	 * Reacquire each frame because frame-slot images change. The TextureRef conveys a generation-checked borrowed resource identity, not ownership. ResourceKey overloads infer the manager's resource domain when unspecified and require this manager's window-local scope; incompatible explicit domains/scopes are errors.
	 *
	 * @return Missing keys are successful unavailable references, with an invalid handle and a once-per-key diagnostic. SkipVisual omits the visual; the fallback policy uses the renderer fallback.
	 * Recoverable initialization, validation, allocation, and storage failures are returned as FlowUiError; no exception escapes this noexcept operation.
	 *
	 * @code{.cpp}
	 * const FlowUi::ResourceKey key{.name = "scene"};
	 * auto result = manager.getTexture(key);
	 * if (!result) {
	 *     throw FlowUi::FlowUiException(result.error());
	 * }
	 * const auto texture = *result;
	 * @endcode
	 */
	[[nodiscard]] Result<TextureRef> getTexture(ResourceKey key) const noexcept;

	/**
	 * @brief Borrow the App-owned Vulkan interop bundle.
	 *
	 * Unwrap result->get() after checking. Keep the borrowed bundle within the App/window lifetime; never destroy its instance/device/allocator/queue handles. This API requires FLOWUI_PUBLIC_VULKAN_INTEROP.
	 *
	 * @return Success contains `std::reference_wrapper<const ViewPortVulkanInterop>`; detached state returns ObjectNotInitialized.
	 * Recoverable initialization, validation, allocation, and storage failures are returned as FlowUiError; no exception escapes this noexcept operation.
	 *
	 * @code{.cpp}
	 * auto result = manager.getVulkanInterop();
	 * if (!result) {
	 *     throw FlowUi::FlowUiException(result.error());
	 * }
	 * const FlowUi::ViewPortVulkanInterop& borrowed = result->get();
	 * @endcode
	 */
	[[nodiscard]] Result<std::reference_wrapper<const ViewPortVulkanInterop>> getVulkanInterop() const noexcept;

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
