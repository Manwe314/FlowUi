#include "internal/Text/TextLineBreaker.hpp"

#include <algorithm>
#include <cctype>

namespace FlowUi::detail::text {

std::vector<TextRange> breakVisualLines(
	std::string_view text,
	const TextLayoutResult& layout,
	float maximumWidth) {
	if (text.empty()) return {TextRange{0, 0}};
	if (!layout.success || layout.clusters.empty() || maximumWidth <= 0.0f) {
		return {TextRange{0, text.size()}};
	}

	std::vector<TextRange> lines;
	size_t firstCluster = 0;
	while (firstCluster < layout.clusters.size()) {
		const float lineOriginX = layout.clusters[firstCluster].xStart;
		size_t endCluster = firstCluster;
		size_t lastBreakCluster = firstCluster;
		bool hasBreak = false;

		while (endCluster < layout.clusters.size()) {
			const TextLayoutCluster& cluster = layout.clusters[endCluster];
			if (cluster.xEnd - lineOriginX > maximumWidth && endCluster > firstCluster) break;
			if (cluster.startByte < text.size()) {
				const unsigned char value = static_cast<unsigned char>(text[cluster.startByte]);
				if (std::isspace(value)) {
					lastBreakCluster = endCluster;
					hasBreak = true;
				}
			}
			++endCluster;
		}

		if (endCluster == layout.clusters.size()) {
			lines.push_back(TextRange{
				layout.clusters[firstCluster].startByte,
				text.size(),
			});
			break;
		}

		size_t breakAfter = endCluster;
		if (hasBreak && lastBreakCluster >= firstCluster) breakAfter = lastBreakCluster + 1u;
		if (breakAfter <= firstCluster) breakAfter = firstCluster + 1u;
		lines.push_back(TextRange{
			layout.clusters[firstCluster].startByte,
			layout.clusters[breakAfter - 1u].endByte,
		});
		firstCluster = breakAfter;
	}
	return lines;
}

} // namespace FlowUi::detail::text
