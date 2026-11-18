#include "../Utf8Main.hpp"
#include <fstream>
#include <limits>
#include <algorithm>
#include <cerrno>
#include <climits>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

#include "FlowUi/Private/resourceService/fontMSDFProvider/FontBakeProfiles.hpp"
#include "FlowUi/Private/resourceService/fontMSDFProvider/FontCpuAssetValidation.hpp"
#include "FlowUi/Private/resourceService/fontMSDFProvider/FontMSDFProviderSelection.hpp"
#include "FlowUi/Public/errorManagement/Exception.hpp"
#include <cstring>

namespace {

constexpr auto bakeProfile = FlowUi::detail::font::offlineFontBakeProfile;
constexpr double kDefaultPixelSize = bakeProfile.pixelSize;
constexpr double kDefaultPxRange = bakeProfile.pixelRange;
constexpr double kDefaultAngleThreshold = bakeProfile.angleThreshold;
constexpr double kDefaultMiterLimit = bakeProfile.miterLimit;
static_assert(FlowUi::detail::font::ActiveFontMSDFProvider::supportsBaking,
			  "Offline font baker requires a baking-capable FontMSDFProvider.");
static_assert(FlowUi::detail::font::ActiveFontMSDFProvider::supportsEncoding,
			  "Offline font baker requires an encoding-capable FontMSDFProvider.");

struct Options {
    std::string inputPath;
    std::string outputPath;
    std::string charsetSpec;
    std::string charsetFilePath;
    double pixelSize = kDefaultPixelSize;
    double pxRange = kDefaultPxRange;
    double angle_threshold = kDefaultAngleThreshold;
    double miter_limit = kDefaultMiterLimit;
    int threadCount = 0; // 0 = auto
    bool showHelp = false;
};

void printUsage(const char *argv0) {
    std::cout
        << "FlowUi Offline Font Baker (MTSDF -> .arfont)\n"
        << "Usage:\n"
        << "  " << argv0 << " --input <font.ttf> --output <font.arfont> [--pixel-size <size>]\n"
        << "          [--charset <inline charset>] [--charset-file <charset.txt>]\n"
        << "          [--px-range <pixels>] [--angle-threshold <radians>]\n"
        << "          [--miter-limit <limit>] [--threads <count>]\n\n"
        << "Required:\n"
        << "  --input, -i        Path to source .ttf/.otf font\n"
        << "  --output, -o       Destination .arfont file path\n\n"
        << "Optional:\n"
        << "  --pixel-size, -s   Glyph size in pixels per em (default: 64)\n"
        << "  --charset, -c      Inline charset specification (msdf-atlas syntax)\n"
        << "  --charset-file     Charset file path (msdf-atlas syntax)\n"
        << "  --px-range         Distance field pixel range (default: 8)\n"
        << "  --angle-threshold  Corner angle threshold in radians (default: 3.0)\n"
        << "  --miter-limit      Bounding box miter limit (default: 1.0)\n"
        << "  --threads, -t      Worker threads (0 = auto)\n"
        << "  --help, -h         Show this help\n\n"
        << "If charset is omitted, printable ASCII + Latin-1 Supplement + common UI symbols\n"
        << "supported by the font are baked.\n";
}

bool parsePositiveDouble(const char *input, double &value) {
    errno = 0;
    char *end = nullptr;
    const double parsed = std::strtod(input, &end);
    if (errno != 0 || end == input || *end != '\0' || !std::isfinite(parsed) || parsed <= 0.0)
        return false;
    value = parsed;
    return true;
}

bool parseNonNegativeInt(const char *input, int &value) {
    errno = 0;
    char *end = nullptr;
    const long parsed = std::strtol(input, &end, 10);
    if (errno != 0 || end == input || *end != '\0' || parsed < 0 || parsed > INT_MAX)
        return false;
    value = static_cast<int>(parsed);
    return true;
}

bool consumeValue(int argc, char **argv, int &argIndex, const char *flag, std::string &out, std::string &error) {
    if (argIndex + 1 >= argc) {
        error = std::string("Missing value for ") + flag;
        return false;
    }
    out = argv[++argIndex];
    return true;
}

bool parseArgs(int argc, char **argv, Options &options, std::string &error) {
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];

        if (arg == "--help" || arg == "-h") {
            options.showHelp = true;
            return true;
        }

        if (arg == "--input" || arg == "-i") {
            if (!consumeValue(argc, argv, i, "--input", options.inputPath, error))
                return false;
            continue;
        }
        if (arg == "--output" || arg == "-o") {
            if (!consumeValue(argc, argv, i, "--output", options.outputPath, error))
                return false;
            continue;
        }
        if (arg == "--charset" || arg == "-c") {
            if (!consumeValue(argc, argv, i, "--charset", options.charsetSpec, error))
                return false;
            continue;
        }
        if (arg == "--charset-file") {
            if (!consumeValue(argc, argv, i, "--charset-file", options.charsetFilePath, error))
                return false;
            continue;
        }
        if (arg == "--pixel-size" || arg == "-s") {
            std::string value;
            if (!consumeValue(argc, argv, i, "--pixel-size", value, error))
                return false;
            if (!parsePositiveDouble(value.c_str(), options.pixelSize)) {
                error = "Invalid --pixel-size value (must be a positive number)";
                return false;
            }
            continue;
        }
        if (arg == "--px-range") {
            std::string value;
            if (!consumeValue(argc, argv, i, "--px-range", value, error))
                return false;
            if (!parsePositiveDouble(value.c_str(), options.pxRange)) {
                error = "Invalid --px-range value (must be a positive number)";
                return false;
            }
            continue;
        }
        if (arg == "--angle-threshold") {
            std::string value;
            if (!consumeValue(argc, argv, i, "--angle-threshold", value, error))
                return false;
            if (!parsePositiveDouble(value.c_str(), options.angle_threshold)) {
                error = "Invalid --angle-threshold value (must be a positive number)";
                return false;
            }
            continue;
        }
        if (arg == "--miter-limit") {
            std::string value;
            if (!consumeValue(argc, argv, i, "--miter-limit", value, error))
                return false;
            if (!parsePositiveDouble(value.c_str(), options.miter_limit)) {
                error = "Invalid --miter-limit value (must be a positive number)";
                return false;
            }
            continue;
        }
        if (arg == "--threads" || arg == "-t") {
            std::string value;
            if (!consumeValue(argc, argv, i, "--threads", value, error))
                return false;
            if (!parseNonNegativeInt(value.c_str(), options.threadCount)) {
                error = "Invalid --threads value (must be an integer >= 0)";
                return false;
            }
            continue;
        }

        error = "Unknown argument: " + arg;
        return false;
    }

    if (options.inputPath.empty()) {
        error = "Missing required --input argument";
        return false;
    }
    if (options.outputPath.empty()) {
        error = "Missing required --output argument";
        return false;
    }
    if (!options.charsetSpec.empty() && !options.charsetFilePath.empty()) {
        error = "Use either --charset or --charset-file, not both";
        return false;
    }

    return true;
}

bool prepareOutputPath(const std::string &outputPath, std::string &error) {
    const auto outPath = flowui::tools::utf8_path(outputPath);
    if (!outPath.has_parent_path())
        return true;

    std::error_code ec;
    std::filesystem::create_directories(outPath.parent_path(), ec);
    if (ec) {
        error = "Failed to create output directory: " + flowui::tools::utf8_text(outPath.parent_path()) + " (" + ec.message() + ")";
        return false;
    }
    return true;
}

[[nodiscard]] bool readOwnedBytes(const std::string& path, std::vector<std::byte>& bytes,
								  bool allowEmpty = false) {
	std::ifstream input(flowui::tools::utf8_path(path), std::ios::binary | std::ios::ate);
	if (!input)
		return false;
	const auto length = input.tellg();
	if (length < 0 || (!allowEmpty && length == 0) ||
		static_cast<uintmax_t>(length) > std::numeric_limits<size_t>::max())
		return false;
	bytes.resize(static_cast<size_t>(length));
	input.seekg(0);
	if (!bytes.empty())
		input.read(reinterpret_cast<char*>(bytes.data()),
				   static_cast<std::streamsize>(bytes.size()));
	return static_cast<bool>(input);
}

/** Resolve only explicit file inclusions; character/range grammar stays in the provider. */
[[nodiscard]] bool expandCharsetFile(const std::filesystem::path& path, std::string& output,
									 std::vector<std::filesystem::path>& activeFiles) {
	std::error_code error;
	const auto canonical = std::filesystem::weakly_canonical(path, error);
	if (error || activeFiles.size() >= 64u ||
		std::find(activeFiles.begin(), activeFiles.end(), canonical) != activeFiles.end())
		return false;
	std::vector<std::byte> bytes;
	if (!readOwnedBytes(flowui::tools::utf8_text(path), bytes, true))
		return false;
	std::string text;
	if (!bytes.empty())
		text.assign(reinterpret_cast<const char*>(bytes.data()), bytes.size());
	if (text.starts_with("\xef\xbb\xbf"))
		text.erase(0u, 3u);
	activeFiles.emplace_back(canonical);
	struct PopActiveFile {
		std::vector<std::filesystem::path>& activeFiles;
		~PopActiveFile() { activeFiles.pop_back(); }
	} activeFile{activeFiles};
	const auto separator = [](char character) {
		return character == ' ' || character == '\t' || character == '\n' || character == '\r' ||
			   character == ',' || character == ';';
	};
	char quote = 0;
	bool escaped = false;
	size_t bracketDepth = 0;
	for (size_t index = 0; index < text.size(); ++index) {
		const char character = text[index];
		if (quote != 0) {
			output += character;
			if (escaped)
				escaped = false;
			else if (character == '\\')
				escaped = true;
			else if (character == quote)
				quote = 0;
		} else if (character == '\'' || character == '"') {
			quote = character;
			output += character;
		} else if (character == '@' &&
				   std::string_view(text).substr(index).starts_with("@include")) {
			if (bracketDepth != 0u || (index != 0u && !separator(text[index - 1u])))
				return false;
			size_t position = index + 8u;
			while (position < text.size() && (text[position] == ' ' || text[position] == '\t' ||
											  text[position] == '\n' || text[position] == '\r'))
				++position;
			if (position == text.size() || text[position] != '"')
				return false;
			++position;
			std::string includeName;
			bool terminated = false;
			for (; position < text.size(); ++position) {
				char included = text[position];
				if (included == '"') {
					terminated = true;
					break;
				}
				if (included == '\\') {
					if (++position >= text.size())
						return false;
					included = text[position];
					switch (included) {
					case '0':
						included = '\0';
						break;
					case 'n':
					case 'N':
						included = '\n';
						break;
					case 'r':
					case 'R':
						included = '\r';
						break;
					case 's':
					case 'S':
						included = ' ';
						break;
					case 't':
					case 'T':
						included = '\t';
						break;
					default:
						break;
					}
				}
				includeName += included;
			}
			if (!terminated || includeName.empty() || includeName.find('\0') != std::string::npos ||
				(position + 1u < text.size() && !separator(text[position + 1u])))
				return false;
			auto includePath = flowui::tools::utf8_path(includeName);
			if (includePath.is_relative())
				includePath = path.parent_path() / includePath;
			output += ' ';
			if (!expandCharsetFile(includePath, output, activeFiles))
				return false;
			output += ' ';
			index = position;
		} else {
			if (character == '[')
				++bracketDepth;
			else if (character == ']' && bracketDepth != 0u)
				--bracketDepth;
			output += character;
		}
	}
	return quote == 0;
}

} // namespace

int main(int argc, char** argv) try {
	Options options;
	std::string error;
	if (!parseArgs(argc, argv, options, error)) {
        std::cerr << "Error: " << error << '\n';
        printUsage(argv[0]);
        return 1;
    }
    if (options.showHelp) {
        printUsage(argv[0]);
        return 0;
    }
    if (!prepareOutputPath(options.outputPath, error)) {
        std::cerr << "Error: " << error << '\n';
        return 1;
    }

	std::vector<std::byte> sourceBytes;
	if (!readOwnedBytes(options.inputPath, sourceBytes)) {
		std::cerr << "Error: Could not read font input\n";
		return 1;
	}
	std::string charsetSpecification = options.charsetSpec;
	auto charsetMode = FlowUi::detail::font::FontCharsetMode::OfflineUi;
	if (!options.charsetFilePath.empty()) {
		std::vector<std::filesystem::path> activeFiles;
		charsetSpecification.clear();
		if (!expandCharsetFile(flowui::tools::utf8_path(options.charsetFilePath),
							   charsetSpecification, activeFiles)) {
			std::cerr << "Error: Failed to read or expand charset file: " << options.charsetFilePath
					  << '\n';
			return 1;
		}
		charsetMode = FlowUi::detail::font::FontCharsetMode::FileSpecification;
	} else if (!options.charsetSpec.empty()) {
		charsetMode = FlowUi::detail::font::FontCharsetMode::InlineSpecification;
	}
	const std::string faceName =
		flowui::tools::utf8_text(flowui::tools::utf8_path(options.outputPath).stem());
	FlowUi::detail::font::ActiveFontMSDFProvider provider;
	auto output = provider.bake(FlowUi::detail::font::FontBakeRequest{
		.sourceBytes = sourceBytes,
		.faceName = faceName,
		.charsetSpecification = charsetSpecification,
		.pixelSize = options.pixelSize,
		.pixelRange = options.pxRange,
		.angleThreshold = options.angle_threshold,
		.miterLimit = options.miter_limit,
		.workerThreadCount = options.threadCount > 0
								 ? static_cast<uint32_t>(options.threadCount)
								 : std::max(1u, std::thread::hardware_concurrency()),
		.weight = 0u,
		.charsetMode = charsetMode});
	if (!output) {
		std::cerr << "Error: Font bake failed: " << FlowUi::errorMessage(output.error().code)
				  << '\n';
		return 1;
	}
	const auto valid = FlowUi::detail::font::validateFontCpuAsset(output->asset);
	if (!valid) {
		std::cerr << "Error: Invalid baked font output: "
				  << FlowUi::errorMessage(valid.error().code) << '\n';
		return 1;
	}
	if (output->requestedCodepointCount > output->loadedGlyphCount) {
		std::cerr << "Warning: " << output->requestedCodepointCount - output->loadedGlyphCount
				  << " charset entries were not found in the font\n";
	}
	const auto encoded = provider.encodeBaked(output->asset);
	if (!encoded) {
		std::cerr << "Error: Font encoding failed: " << FlowUi::errorMessage(encoded.error().code)
				  << '\n';
		return 1;
	}
	std::ofstream destination(flowui::tools::utf8_path(options.outputPath), std::ios::binary);
	if (encoded->size() > static_cast<size_t>(std::numeric_limits<std::streamsize>::max()) ||
		!destination) {
		std::cerr << "Error: Failed to write .arfont output: " << options.outputPath << '\n';
		return 1;
	}
	destination.write(reinterpret_cast<const char*>(encoded->data()),
					  static_cast<std::streamsize>(encoded->size()));
	destination.close();
	if (!destination) {
		std::cerr << "Error: Failed to write .arfont output: " << options.outputPath << '\n';
		return 1;
	}
	const auto& variant = output->asset.variants.front();
	std::cout << "Generated .arfont atlas: " << options.outputPath << '\n'
			  << "  glyphs: " << output->loadedGlyphCount << '\n'
			  << "  atlas: " << output->asset.atlas.width << "x" << output->asset.atlas.height
			  << '\n'
			  << "  pixel size: " << options.pixelSize << '\n'
			  << "  px range: [" << variant.distanceRangeMiddle - 0.5f * variant.distanceRange
			  << ", " << variant.distanceRangeMiddle + 0.5f * variant.distanceRange << "]\n";
	return 0;
} catch (const std::exception& exception) {
	std::cerr << "Error: " << exception.what() << '\n';
	return 1;
} catch (...) {
	std::cerr << "Error: Unexpected font baker failure\n";
	return 1;
}
