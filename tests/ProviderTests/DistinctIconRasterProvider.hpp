#pragma once

#include <array>
#include <atomic>
#include <cstring>
#include <utility>

#include "FlowUi/Private/ApiBoundary.hpp"
#include "FlowUi/Private/resourceService/iconRasterProvider/IconRasterProvider.hpp"

namespace FlowUi::test {

/** Working vendor-free test provider with deliberately distinct pixels and aspect ratio. */
class DistinctIconRasterProvider {
public:
	static constexpr uint32_t contractRevision = detail::icon::iconRasterContractRevision;
	inline static std::atomic_uint64_t parseCalls{0};
	inline static std::atomic_uint64_t rasterCalls{0};
	inline static std::atomic_uint64_t liveDocuments{0};
	inline static std::atomic_uint64_t destroyedDocuments{0};
	inline static std::atomic_bool corruptRaster{false};

	class Document {
	public:
		explicit Document(std::vector<std::byte> source) noexcept : source_(std::move(source)) {
			liveDocuments.fetch_add(1, std::memory_order_relaxed);
		}
		~Document() noexcept { release(); }
		Document(const Document&) = delete;
		Document& operator=(const Document&) = delete;
		Document(Document&& other) noexcept
			: source_(std::move(other.source_)), active_(std::exchange(other.active_, false)) {}
		Document& operator=(Document&& other) noexcept {
			if (this == &other)
				return *this;
			release();
			source_ = std::move(other.source_);
			active_ = std::exchange(other.active_, false);
			return *this;
		}

	private:
		friend class DistinctIconRasterProvider;
		void release() noexcept {
			if (!std::exchange(active_, false))
				return;
			liveDocuments.fetch_sub(1, std::memory_order_relaxed);
			destroyedDocuments.fetch_add(1, std::memory_order_relaxed);
		}
		std::vector<std::byte> source_;
		bool active_ = true;
	};

	[[nodiscard]] Result<Document> parse(std::span<const std::byte> source) noexcept {
		parseCalls.fetch_add(1, std::memory_order_relaxed);
		return detail::catchResult<Result<Document>>(
			ErrorSite::IconParseSource, [&]() -> Result<Document> {
				if (source.empty())
					return unexpectedError(
						makeError(ErrorCode::IconSourceInvalid, ErrorSite::IconParseSource));
				return Document{std::vector<std::byte>(source.begin(), source.end())};
			});
	}
	[[nodiscard]] detail::icon::IconDimensions
	intrinsicSize(const Document& document) const noexcept {
		return document.active_ ? detail::icon::IconDimensions{8.0f, 4.0f}
								: detail::icon::IconDimensions{};
	}
	[[nodiscard]] detail::icon::IconDimensions
	contentSize(const Document& document) const noexcept {
		return intrinsicSize(document);
	}
	[[nodiscard]] Result<detail::Rgba8Image>
	rasterize(const Document& document, const detail::icon::IconRasterRequest& request) noexcept {
		rasterCalls.fetch_add(1, std::memory_order_relaxed);
		return detail::catchResult<Result<detail::Rgba8Image>>(
			ErrorSite::IconRasterize, [&]() -> Result<detail::Rgba8Image> {
				const auto count = detail::rgba8ByteSize(request.width, request.height);
				if (!document.active_ || !count.valid)
					return unexpectedError(
						makeError(ErrorCode::IconRasterInvalid, ErrorSite::IconRasterize));
				detail::Rgba8Image image{.pixels = std::vector<std::byte>(count.value),
										 .width = request.width,
										 .height = request.height};
				constexpr std::array color{std::byte{17}, std::byte{119}, std::byte{221},
										   std::byte{128}};
				for (size_t offset = 0; offset < image.pixels.size(); offset += color.size())
					std::memcpy(image.pixels.data() + offset, color.data(), color.size());
				if (corruptRaster.load(std::memory_order_relaxed))
					image.pixels.pop_back();
				return image;
			});
	}
};

static_assert(detail::icon::IconRasterProvider<DistinctIconRasterProvider>);
static_assert(!std::is_default_constructible_v<DistinctIconRasterProvider::Document>);

} // namespace FlowUi::test
