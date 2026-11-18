#pragma once

#include <memory>
#include <span>

#include "FlowUi/Private/resourceService/iconRasterProvider/IconRasterProvider.hpp"

namespace FlowUi::detail::icon {

/** Default CPU SVG parser/rasterizer; native types stay in its implementation. */
class PlutoSvgIconRasterProvider {
public:
	static constexpr uint32_t contractRevision = iconRasterContractRevision;

	/** Movable ownership of a native document and the stable source bytes it borrows. */
	class Document {
	public:
		~Document() noexcept;
		Document(Document&&) noexcept;
		Document& operator=(Document&&) noexcept;
		Document(const Document&) = delete;
		Document& operator=(const Document&) = delete;

	private:
		friend class PlutoSvgIconRasterProvider;
		struct Impl;
		explicit Document(std::unique_ptr<Impl> implementation) noexcept;
		std::unique_ptr<Impl> impl_;
	};

	/** Parse and own the supplied SVG bytes; no input pointer is retained. */
	[[nodiscard]] Result<Document> parse(std::span<const std::byte> source) noexcept;
	/** Observe finite positive authored dimensions, or zero when unavailable. */
	[[nodiscard]] IconDimensions intrinsicSize(const Document& document) const noexcept;
	/** Observe finite positive content extents computed during parsing. */
	[[nodiscard]] IconDimensions contentSize(const Document& document) const noexcept;
	/** Render exactly the requested extent to owned tight straight-alpha RGBA8 bytes. */
	[[nodiscard]] Result<Rgba8Image> rasterize(const Document& document,
		const IconRasterRequest& request) noexcept;
};

static_assert(IconRasterProvider<PlutoSvgIconRasterProvider>);

} // namespace FlowUi::detail::icon
