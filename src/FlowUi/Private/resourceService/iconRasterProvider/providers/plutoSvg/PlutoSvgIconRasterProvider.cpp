#include "FlowUi/Private/resourceService/iconRasterProvider/providers/plutoSvg/PlutoSvgIconRasterProvider.hpp"
#include "FlowUi/Private/ApiBoundary.hpp"
#include "FlowUi/Private/resourceService/iconRasterProvider/providers/plutoSvg/PlutoSvgPixelConversions.hpp"

#include <cmath>
#include <cstring>
#include <limits>
#include <string>
#include <utility>

#include <plutosvg.h>

namespace FlowUi::detail::icon {
namespace {
struct PlutoDocumentDeleter {
	void operator()(plutosvg_document_t* document) const noexcept {
		plutosvg_document_destroy(document);
	}
};
struct PlutoSurfaceDeleter {
	void operator()(plutovg_surface_t* surface) const noexcept { plutovg_surface_destroy(surface); }
};

[[nodiscard]] float positiveDimension(float value) noexcept {
	return std::isfinite(value) && value > 0.0f ? value : 0.0f;
}
} // namespace

struct PlutoSvgIconRasterProvider::Document::Impl {
	std::string source;
	std::unique_ptr<plutosvg_document_t, PlutoDocumentDeleter> nativeDocument;
	IconDimensions intrinsicDimensions{};
	IconDimensions contentDimensions{};
};

PlutoSvgIconRasterProvider::Document::Document(std::unique_ptr<Impl> implementation) noexcept
	: impl_(std::move(implementation)) {}
PlutoSvgIconRasterProvider::Document::~Document() noexcept = default;
PlutoSvgIconRasterProvider::Document::Document(Document&&) noexcept = default;
PlutoSvgIconRasterProvider::Document&
PlutoSvgIconRasterProvider::Document::operator=(Document&&) noexcept = default;

Result<PlutoSvgIconRasterProvider::Document>
PlutoSvgIconRasterProvider::parse(std::span<const std::byte> source) noexcept {
	return detail::catchResult<Result<Document>>(
		ErrorSite::IconParseSource, [&]() -> Result<Document> {
			if (source.empty() ||
				source.size() > static_cast<size_t>(std::numeric_limits<int>::max())) {
				return unexpectedError(
					makeError(ErrorCode::IconSourceInvalid, ErrorSite::IconParseSource));
			}
			auto implementation = std::make_unique<Document::Impl>();
			implementation->source.assign(reinterpret_cast<const char*>(source.data()),
										  source.size());
			implementation->nativeDocument.reset(plutosvg_document_load_from_data(
				implementation->source.data(), static_cast<int>(source.size()), -1.0f, -1.0f,
				nullptr, nullptr));
			if (!implementation->nativeDocument) {
				return unexpectedError(
					makeError(ErrorCode::IconSourceInvalid, ErrorSite::IconParseSource));
			}
			implementation->intrinsicDimensions = {positiveDimension(plutosvg_document_get_width(
													   implementation->nativeDocument.get())),
												   positiveDimension(plutosvg_document_get_height(
													   implementation->nativeDocument.get()))};
			plutovg_rect_t extents{};
			if (plutosvg_document_extents(implementation->nativeDocument.get(), nullptr,
										  &extents)) {
				implementation->contentDimensions = {positiveDimension(extents.w),
													 positiveDimension(extents.h)};
			}
			return Document{std::move(implementation)};
		});
}

IconDimensions PlutoSvgIconRasterProvider::intrinsicSize(const Document& document) const noexcept {
	return document.impl_ ? document.impl_->intrinsicDimensions : IconDimensions{};
}
IconDimensions PlutoSvgIconRasterProvider::contentSize(const Document& document) const noexcept {
	return document.impl_ ? document.impl_->contentDimensions : IconDimensions{};
}

Result<Rgba8Image>
PlutoSvgIconRasterProvider::rasterize(const Document& document,
									  const IconRasterRequest& request) noexcept {
	return detail::catchResult<Result<Rgba8Image>>(
		ErrorSite::IconRasterize, [&]() -> Result<Rgba8Image> {
			const auto outputBytes = rgba8ByteSize(request.width, request.height);
			if (!document.impl_ || !document.impl_->nativeDocument || !outputBytes.valid ||
				request.width > static_cast<uint32_t>(std::numeric_limits<int>::max()) ||
				request.height > static_cast<uint32_t>(std::numeric_limits<int>::max())) {
				return unexpectedError(
					makeError(ErrorCode::IconRasterInvalid, ErrorSite::IconRasterize));
			}
			std::unique_ptr<plutovg_surface_t, PlutoSurfaceDeleter> surface{
				plutosvg_document_render_to_surface(
					document.impl_->nativeDocument.get(), nullptr, static_cast<int>(request.width),
					static_cast<int>(request.height), nullptr, nullptr, nullptr)};
			if (!surface) {
				return unexpectedError(
					makeError(ErrorCode::IconRasterizationFailed, ErrorSite::IconRasterize));
			}
			const int width = plutovg_surface_get_width(surface.get());
			const int height = plutovg_surface_get_height(surface.get());
			const int stride = plutovg_surface_get_stride(surface.get());
			const unsigned char* data = plutovg_surface_get_data(surface.get());
			const size_t rowBytes = static_cast<size_t>(request.width) * 4u;
			if (width != static_cast<int>(request.width) ||
				height != static_cast<int>(request.height) || stride <= 0 || !data ||
				static_cast<size_t>(stride) < rowBytes ||
				static_cast<size_t>(stride) > std::numeric_limits<size_t>::max() / request.height) {
				return unexpectedError(
					makeError(ErrorCode::IconRasterInvalid, ErrorSite::IconRasterize));
			}
			Rgba8Image output{.pixels = std::vector<std::byte>(outputBytes.value),
							  .width = request.width,
							  .height = request.height};
			const auto nativePixels = std::span{reinterpret_cast<const std::byte*>(data),
												static_cast<size_t>(stride) * request.height};
			if (!copyNativeArgbRows(nativePixels, static_cast<size_t>(stride), request.width,
									request.height, output.pixels)) {
				return unexpectedError(
					makeError(ErrorCode::IconRasterInvalid, ErrorSite::IconRasterize));
			}
			return output;
		});
}

} // namespace FlowUi::detail::icon
