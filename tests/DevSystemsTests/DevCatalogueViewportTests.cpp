#ifdef NDEBUG
#undef NDEBUG
#endif
#include "FlowUi/App.hpp"
#include "devSystems/devTooling/DevTooling.hpp"
#include "managers/UiManager.hpp"
#include "managers/ViewPortManager.hpp"
#include <algorithm>
#include <array>
#include <cassert>

namespace {
constexpr std::string_view viewport_key = "catalogue-test/natural-output";

[[nodiscard]] FlowUi::DevViewportInfo observation(const FlowUi::App& app, FlowUi::WindowId window) {
	const auto entries = app.devViewportSnapshot(window);
	const auto found = std::ranges::find(entries, viewport_key, &FlowUi::DevViewportInfo::key);
	assert(found != entries.end());
	return *found;
}

void build(FlowUi::App& app, FlowUi::WindowId window, bool source,
		   const FlowUi::DevViewportInfo* immutable = nullptr) {
	auto& ui = app.ui(window);
	Clay_ElementDeclaration root{};
	root.layout.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0)};
	CLAY(ui.toClaySID("catalogue-capture-test"), root) {
		if (source) {
			Clay_ElementDeclaration image{};
			image.layout.sizing = {CLAY_SIZING_FIXED(64), CLAY_SIZING_FIXED(48)};
			image.image.imageData = ui.imageData(app.viewPorts(window).getTexture(viewport_key));
			CLAY(ui.toClaySID("natural-source"), image) {}
		}
		if (immutable) {
			Clay_ElementDeclaration image{};
			image.layout.sizing = {CLAY_SIZING_FIXED(128), CLAY_SIZING_FIXED(96)};
			image.image.imageData = ui.imageData(FlowUi::TextureRef{
				.handle = immutable->capture_texture,
				.sourceWidth = static_cast<int32_t>(immutable->captured_width),
				.sourceHeight = static_cast<int32_t>(immutable->captured_height)});
			CLAY(ui.toClaySID("immutable-observation"), image) {}
		}
	}
}

void render(FlowUi::App& app, FlowUi::WindowId window, bool source = false,
			const FlowUi::DevViewportInfo* immutable = nullptr) {
	assert(window == app.mainWindowId() ? app.beginFrame() : app.beginFrame(window));
	build(app, window, source, immutable);
	assert(app.endFrame(window));
	assert(app.drawFrame(window));
}

[[nodiscard]] FlowUi::DevViewportInfo wait_for_copy(FlowUi::App& app,
													FlowUi::WindowId source_window,
													FlowUi::TextureHandle previous = {}) {
	auto latest = observation(app, source_window);
	for (unsigned frame = 0; frame < 16; ++frame) {
		render(app, app.mainWindowId());
		render(app, source_window);
		latest = observation(app, source_window);
		if (latest.capture_texture && latest.capture_texture != previous)
			return latest;
	}
	assert(latest.capture_texture && latest.capture_texture != previous);
	return latest;
}

void verify_natural_render_copy_and_retention() {
	using namespace FlowUi;
	unsigned render_callbacks = 0;
	AppConfig configuration;
	configuration.window.title = "Catalogue immutable viewport verification";
	configuration.window.width = 320;
	configuration.window.height = 240;
	auto app = makeApplication(configuration);
	WindowConfigOverrides overrides;
	overrides.title = "Catalogue source viewport";
	overrides.width = 300;
	overrides.height = 200;
	const auto created_window = app.createWindow(overrides);
	assert(created_window);
	const auto source_window = *created_window;
	assert(app.viewPorts(source_window).create(viewport_key));
	auto* viewport = app.viewPorts(source_window).getViewPort(viewport_key);
	assert(viewport);
	viewport->setClearColor(.9f, .1f, .1f, 1);
	viewport->setRenderCallback([&](const ViewPortRenderContext&) { ++render_callbacks; });
	const auto registered = observation(app, source_window);
	assert(registered.capture_supported && !registered.capture_texture);
	assert(app.request_dev_viewport_capture(source_window, viewport_key));
	assert(app.request_dev_viewport_capture(source_window, viewport_key));
	for (unsigned frame = 0; frame < 3; ++frame) {
		render(app, app.mainWindowId());
		render(app, source_window);
		const auto waiting = observation(app, source_window);
		assert(waiting.capture_pending && !waiting.capture_texture);
		assert(waiting.width == registered.width && waiting.height == registered.height);
		assert(render_callbacks == 0);
	}
	render(app, app.mainWindowId());
	assert(app.beginFrame(source_window));
	build(app, source_window, true);
	assert(app.endFrame(source_window));
	assert(!observation(app, source_window).capture_texture);
	assert(render_callbacks == 0);
	assert(app.drawFrame(source_window));
	assert(render_callbacks == 1);
	auto frozen = wait_for_copy(app, source_window);
	assert(frozen.captured_frame && frozen.captured_submission);
	assert(frozen.captured_generation == frozen.generation);
	assert(frozen.captured_width == frozen.width && frozen.captured_height == frozen.height);
	const auto frozen_backend_texture = frozen.capture_texture;
	const auto retained =
		app.retain_dev_viewport_capture(source_window, viewport_key, frozen_backend_texture);
	assert(retained && *retained != frozen_backend_texture);
	frozen.capture_texture = *retained;
	const std::array frozen_textures{frozen.capture_texture};
	auto lease = app.devTooling().catalogues().acquireInspectionLease(frozen_textures);
	assert(lease.resourceCount() == 1);
	for (unsigned frame = 0; frame < 3; ++frame) {
		render(app, app.mainWindowId(), false, &frozen);
		render(app, source_window);
		assert(render_callbacks == 1);
		assert(observation(app, source_window).capture_texture == frozen_backend_texture);
	}
	assert(app.request_dev_viewport_capture(source_window, viewport_key));
	viewport->setClearColor(.1f, .2f, .9f, 1);
	render(app, app.mainWindowId(), false, &frozen);
	render(app, source_window, true);
	assert(render_callbacks == 2);
	const auto newer = wait_for_copy(app, source_window, frozen_backend_texture);
	assert(newer.capture_texture != frozen_backend_texture);
	assert(newer.captured_frame > frozen.captured_frame);
	assert(newer.captured_submission > frozen.captured_submission);
	assert(newer.width == frozen.width && newer.height == frozen.height);
	render(app, app.mainWindowId(), false, &frozen);
	assert(render_callbacks == 2);

	// A release between preparation and submission must keep the recorded copy alive.
	app.release_dev_viewport_capture(source_window, viewport_key);
	assert(app.request_dev_viewport_capture(source_window, viewport_key));
	render(app, app.mainWindowId(), false, &frozen);
	assert(app.beginFrame(source_window));
	build(app, source_window, true);
	assert(app.endFrame(source_window));
	app.release_dev_viewport_capture(source_window, viewport_key);
	assert(app.drawFrame(source_window));
	for (unsigned frame = 0; frame < 8; ++frame) {
		render(app, app.mainWindowId(), false, &frozen);
		render(app, source_window);
	}
	assert(render_callbacks == 3);
	assert(!observation(app, source_window).capture_texture);
	const auto removed = app.viewPorts(source_window).remove(viewport_key);
	assert(removed && *removed);
	assert(app.devViewportSnapshot(source_window).empty());
	render(app, app.mainWindowId(), false, &frozen);
	assert(app.destroyWindow(source_window));
	assert(app.devViewportSnapshot(source_window).empty());
	assert(!app.request_dev_viewport_capture(source_window, viewport_key));
	app.release_dev_viewport_capture(source_window, viewport_key);
	auto surviving_binding = app.devTooling().catalogues().acquireInspectionLease(frozen_textures);
	assert(surviving_binding.resourceCount() == 1);
	surviving_binding = {};
	// The AppShared immutable copy remains sampleable after both source identities disappear.
	for (unsigned frame = 0; frame < 4; ++frame)
		render(app, app.mainWindowId(), false, &frozen);
	assert(render_callbacks == 3);
	lease = {};
	assert(app.release_dev_viewport_capture_texture(frozen.capture_texture));
}
} // namespace

int main() {
	verify_natural_render_copy_and_retention();
}
