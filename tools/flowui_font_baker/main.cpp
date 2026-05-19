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

#include <msdf-atlas-gen/msdf-atlas-gen.h>

namespace {

constexpr double kDefaultPixelSize = 32.0;
constexpr double kDefaultPxRange = 6.0;
constexpr double kDefaultAngleThreshold = 3.0;
constexpr double kDefaultMiterLimit = 1.0;
constexpr unsigned long long kLcgMultiplier = 6364136223846793005ull;
constexpr unsigned long long kLcgIncrement = 1442695040888963407ull;

struct Options {
    std::string inputPath;
    std::string outputPath;
    std::string charsetSpec;
    std::string charsetFilePath;
    double pixelSize = kDefaultPixelSize;
    double pxRange = kDefaultPxRange;
    int threadCount = 0; // 0 = auto
    bool showHelp = false;
    bool hasPixelSize = false;
};

struct FreetypeGuard {
    msdfgen::FreetypeHandle *handle = nullptr;

    ~FreetypeGuard() {
        if (handle)
            msdfgen::deinitializeFreetype(handle);
    }
};

struct FontGuard {
    msdfgen::FontHandle *handle = nullptr;

    ~FontGuard() {
        if (handle)
            msdfgen::destroyFont(handle);
    }
};

void printUsage(const char *argv0) {
    std::cout
        << "FlowUi Offline Font Baker (MSDF -> .arfont)\n"
        << "Usage:\n"
        << "  " << argv0 << " --input <font.ttf> --output <font.arfont> --pixel-size <size>\n"
        << "          [--charset <inline charset>] [--charset-file <charset.txt>]\n"
        << "          [--px-range <pixels>] [--threads <count>]\n\n"
        << "Required:\n"
        << "  --input, -i        Path to source .ttf/.otf font\n"
        << "  --output, -o       Destination .arfont file path\n"
        << "  --pixel-size, -s   Glyph size in pixels per em (must be > 0)\n\n"
        << "Optional:\n"
        << "  --charset, -c      Inline charset specification (msdf-atlas syntax)\n"
        << "  --charset-file     Charset file path (msdf-atlas syntax)\n"
        << "  --px-range         Distance field pixel range (default: 6)\n"
        << "  --threads, -t      Worker threads (0 = auto)\n"
        << "  --help, -h         Show this help\n\n"
        << "If charset is omitted, printable ASCII is used.\n";
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
            options.hasPixelSize = true;
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
    if (!options.hasPixelSize) {
        error = "Missing required --pixel-size argument";
        return false;
    }
    if (!options.charsetSpec.empty() && !options.charsetFilePath.empty()) {
        error = "Use either --charset or --charset-file, not both";
        return false;
    }

    return true;
}

bool prepareOutputPath(const std::string &outputPath, std::string &error) {
    const std::filesystem::path outPath(outputPath);
    if (!outPath.has_parent_path())
        return true;

    std::error_code ec;
    std::filesystem::create_directories(outPath.parent_path(), ec);
    if (ec) {
        error = "Failed to create output directory: " + outPath.parent_path().string() + " (" + ec.message() + ")";
        return false;
    }
    return true;
}

bool buildCharset(const Options &options, msdf_atlas::Charset &charset, std::string &error) {
    if (!options.charsetFilePath.empty()) {
        if (!charset.load(options.charsetFilePath.c_str(), false)) {
            error = "Failed to parse charset file: " + options.charsetFilePath;
            return false;
        }
        return true;
    }

    if (!options.charsetSpec.empty()) {
        if (!charset.parse(options.charsetSpec.c_str(), options.charsetSpec.size(), false)) {
            // Fallback: treat the provided string as literal text and include every UTF-8 codepoint.
            std::vector<msdf_atlas::unicode_t> decoded;
            msdf_atlas::utf8Decode(decoded, options.charsetSpec.c_str());
            if (decoded.empty()) {
                error = "Failed to parse inline charset specification";
                return false;
            }
            for (msdf_atlas::unicode_t codepoint : decoded) {
                if (codepoint)
                    charset.add(codepoint);
            }
        }
        return true;
    }

    charset = msdf_atlas::Charset::ASCII;
    return true;
}

} // namespace

int main(int argc, char **argv) {
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

    FreetypeGuard freetype;
    freetype.handle = msdfgen::initializeFreetype();
    if (!freetype.handle) {
        std::cerr << "Error: Failed to initialize FreeType\n";
        return 1;
    }

    FontGuard font;
    font.handle = msdfgen::loadFont(freetype.handle, options.inputPath.c_str());
    if (!font.handle) {
        std::cerr << "Error: Failed to load font file: " << options.inputPath << '\n';
        return 1;
    }

    msdf_atlas::Charset charset;
    if (!buildCharset(options, charset, error)) {
        std::cerr << "Error: " << error << '\n';
        return 1;
    }

    std::vector<msdf_atlas::GlyphGeometry> glyphs;
    msdf_atlas::FontGeometry fontGeometry(&glyphs);
    const int glyphsLoaded = fontGeometry.loadCharset(
        font.handle,
        1.0,
        charset,
        false,
        true
    );

    if (glyphsLoaded < 0) {
        std::cerr << "Error: Failed to load glyph geometry from font\n";
        return 1;
    }
    if (glyphsLoaded == 0 || glyphs.empty()) {
        std::cerr << "Error: No glyphs were loaded for the selected charset\n";
        return 1;
    }
    if (glyphsLoaded < static_cast<int>(charset.size())) {
        std::cerr << "Warning: " << (charset.size() - static_cast<size_t>(glyphsLoaded))
                  << " charset entries were not found in the font\n";
    }

    unsigned long long glyphSeed = 0;
    for (msdf_atlas::GlyphGeometry &glyph : glyphs) {
        glyphSeed = glyphSeed * kLcgMultiplier + kLcgIncrement;
        glyph.edgeColoring(&msdfgen::edgeColoringInkTrap, kDefaultAngleThreshold, glyphSeed);
    }

    msdf_atlas::TightAtlasPacker packer;
    packer.setDimensionsConstraint(msdf_atlas::DimensionsConstraint::MULTIPLE_OF_FOUR_SQUARE);
    packer.setSpacing(2);
    packer.setScale(options.pixelSize);
    packer.setPixelRange(options.pxRange);
    packer.setMiterLimit(kDefaultMiterLimit);
    packer.setOriginPixelAlignment(false, true);

    const int remaining = packer.pack(glyphs.data(), glyphs.size());
    if (remaining < 0) {
        std::cerr << "Error: Failed to pack glyphs into an atlas\n";
        return 1;
    }
    if (remaining > 0) {
        std::cerr << "Error: Could not fit " << remaining << " glyphs into the atlas\n";
        return 1;
    }

    int atlasWidth = 0;
    int atlasHeight = 0;
    packer.getDimensions(atlasWidth, atlasHeight);
    if (atlasWidth <= 0 || atlasHeight <= 0) {
        std::cerr << "Error: Invalid atlas dimensions produced by packer\n";
        return 1;
    }

    msdf_atlas::GeneratorAttributes attributes;
    attributes.config.overlapSupport = true;
    attributes.scanlinePass = true;

    using MtsdfGenerator = msdf_atlas::ImmediateAtlasGenerator<
        float,
        4,
        msdf_atlas::mtsdfGenerator,
        msdf_atlas::BitmapAtlasStorage<msdf_atlas::byte, 4>
    >;

    MtsdfGenerator generator(atlasWidth, atlasHeight);
    generator.setAttributes(attributes);
    if (options.threadCount > 0)
        generator.setThreadCount(options.threadCount);
    else
        generator.setThreadCount(static_cast<int>(std::max(1u, std::thread::hardware_concurrency())));
    generator.generate(glyphs.data(), glyphs.size());

    const msdfgen::BitmapConstSection<msdf_atlas::byte, 4> atlasBitmap =
        static_cast<msdfgen::BitmapConstSection<msdf_atlas::byte, 4>>(generator.atlasStorage());

    const msdfgen::Range finalPxRange = packer.getPixelRange();

    msdf_atlas::ArteryFontExportProperties exportProperties = {};
    exportProperties.fontSize = packer.getScale();
    exportProperties.pxRange = finalPxRange;
    exportProperties.imageType = msdf_atlas::ImageType::MTSDF;
#ifdef MSDFGEN_DISABLE_PNG
    exportProperties.imageFormat = msdf_atlas::ImageFormat::BINARY;
#else
    exportProperties.imageFormat = msdf_atlas::ImageFormat::PNG;
#endif

    if (!msdf_atlas::exportArteryFont<float>(&fontGeometry, 1, atlasBitmap, options.outputPath.c_str(), exportProperties)) {
        std::cerr << "Error: Failed to write .arfont output: " << options.outputPath << '\n';
        return 1;
    }

    std::cout << "Generated .arfont atlas: " << options.outputPath << '\n'
              << "  glyphs: " << glyphsLoaded << '\n'
              << "  atlas: " << atlasWidth << "x" << atlasHeight << '\n'
              << "  pixel size: " << packer.getScale() << '\n'
              << "  px range: [" << finalPxRange.lower << ", " << finalPxRange.upper << "]\n";
    return 0;
}
