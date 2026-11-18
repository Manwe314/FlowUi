"""Compile private CPU-provider contracts and verify intentional diagnostics."""

import argparse
import json
from pathlib import Path
import subprocess


PREAMBLE = r'''
#include "FlowUi/Private/textLayoutService/TextLayoutServiceConcept.hpp"
#include "FlowUi/Private/resourceService/fontMSDFProvider/FontMSDFProvider.hpp"
#include "FlowUi/Private/resourceService/iconRasterProvider/IconRasterProvider.hpp"
#include "FlowUi/Private/textLayoutService/providers/basic/BasicTextLayoutProvider.hpp"
#include "FlowUi/Private/resourceService/fontMSDFProvider/providers/msdfAtlas/MsdfAtlasFontProvider.hpp"
#include "FlowUi/Private/resourceService/iconRasterProvider/providers/plutoSvg/PlutoSvgIconRasterProvider.hpp"
using namespace FlowUi;
namespace text = FlowUi::detail::text;
namespace font = FlowUi::detail::font;
namespace icon = FlowUi::detail::icon;
using FlowUi::detail::Rgba8Image;

struct CompleteText {
    static constexpr uint32_t contractRevision = text::textLayoutContractRevision;
    CompleteText() noexcept = default;
    CompleteText(const CompleteText&) = delete;
    CompleteText(CompleteText&&) = delete;
    Result<text::TextLayoutResult> layout(const text::TextProviderInput&, const Font::FontFaceData&) noexcept;
};
struct CompleteFont {
    static constexpr uint32_t contractRevision = font::fontMsdfContractRevision;
    static constexpr bool supportsBaking = true;
    static constexpr bool supportsEncoding = true;
    CompleteFont() noexcept = default;
    CompleteFont(const CompleteFont&) = delete;
    CompleteFont(CompleteFont&&) = delete;
    Result<font::FontCpuAsset> loadBaked(std::span<const std::byte>) noexcept;
    Result<font::FontBakeOutput> bake(const font::FontBakeRequest&) noexcept;
    Result<std::vector<std::byte>> encodeBaked(const font::FontCpuAsset&) noexcept;
};
struct CompleteIcon {
    struct Document {
        Document() = delete;
        Document(const Document&) = delete;
        Document& operator=(const Document&) = delete;
        Document(Document&&) noexcept;
        Document& operator=(Document&&) noexcept;
        ~Document() noexcept;
    };
    static constexpr uint32_t contractRevision = icon::iconRasterContractRevision;
    CompleteIcon() noexcept = default;
    CompleteIcon(const CompleteIcon&) = delete;
    CompleteIcon(CompleteIcon&&) = delete;
    Result<Document> parse(std::span<const std::byte>) noexcept;
    icon::IconDimensions intrinsicSize(const Document&) const noexcept;
    icon::IconDimensions contentSize(const Document&) const noexcept;
    Result<Rgba8Image> rasterize(const Document&, const icon::IconRasterRequest&) noexcept;
};
static_assert(text::TextLayoutServiceConcept<CompleteText>);
static_assert(font::FontMSDFProvider<CompleteFont>);
static_assert(icon::IconRasterProvider<CompleteIcon>);
static_assert(text::TextLayoutServiceConcept<text::BasicTextLayoutProvider>);
static_assert(font::FontMSDFProvider<font::MsdfAtlasFontProvider>);
static_assert(icon::IconRasterProvider<icon::PlutoSvgIconRasterProvider>);
'''


def fixture(domain, name, declarations):
    concepts = {"Text": "text::TextLayoutServiceConcept", "Font": "font::FontMSDFProvider",
                "Icon": "icon::IconRasterProvider"}
    if domain == "Icon" and name.endswith("document"):
        declarations += " Result<Document> parse(std::span<const std::byte>) noexcept; icon::IconDimensions intrinsicSize(const Document&) const noexcept; icon::IconDimensions contentSize(const Document&) const noexcept; Result<Rgba8Image> rasterize(const Document&, const icon::IconRasterRequest&) noexcept;"
    return (f"{domain}-{name}", f"struct Rejected : Complete{domain} {{ {declarations} }};\n"
            f'static_assert({concepts[domain]}<Rejected>, "INTENDED_{domain.upper()}_PROVIDER_REJECTION");\n')


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--source", type=Path, required=True)
    parser.add_argument("--build", type=Path, required=True)
    parser.add_argument("--compiler", required=True)
    arguments = parser.parse_args()
    arguments.source = arguments.source.resolve()
    arguments.build = arguments.build.resolve()
    evidence = arguments.source / ".flowui/agentic/verification/phase3-providers/contracts"
    evidence.mkdir(parents=True, exist_ok=True)
    cases = []
    for domain in ("Text", "Font", "Icon"):
        cases.extend([
            fixture(domain, "wrong-revision", "static constexpr uint32_t contractRevision = 2;"),
            fixture(domain, "wrong-revision-type", "static constexpr uint64_t contractRevision = 1;"),
            fixture(domain, "runtime-revision", "inline static uint32_t contractRevision = 1;"),
            fixture(domain, "missing-revision", f"private: using Complete{domain}::contractRevision;"),
            fixture(domain, "polymorphic", "virtual ~Rejected() noexcept;"),
            fixture(domain, "throwing-construction", "Rejected() noexcept(false);"),
            fixture(domain, "throwing-destruction", "~Rejected() noexcept(false);"),
        ])
    cases.extend([
        fixture("Text", "missing-layout", "void layout() = delete;"),
        fixture("Text", "wrong-return", "bool layout(const text::TextProviderInput&, const Font::FontFaceData&) noexcept;"),
        fixture("Text", "throwing-layout", "Result<text::TextLayoutResult> layout(const text::TextProviderInput&, const Font::FontFaceData&);"),
        fixture("Text", "wrong-receiver", "Result<text::TextLayoutResult> layout(const text::TextProviderInput&, const Font::FontFaceData&) const && noexcept;"),
        fixture("Font", "missing-load", "void loadBaked() = delete;"),
        fixture("Font", "missing-bake", "void bake() = delete;"),
        fixture("Font", "missing-encode", "void encodeBaked() = delete;"),
        fixture("Font", "wrong-return", "bool loadBaked(std::span<const std::byte>) noexcept;"),
        fixture("Font", "throwing-bake", "Result<font::FontBakeOutput> bake(const font::FontBakeRequest&);"),
        fixture("Font", "wrong-receiver", "Result<font::FontCpuAsset> loadBaked(std::span<const std::byte>) const && noexcept;"),
        fixture("Font", "missing-baking-capability", "private: using CompleteFont::supportsBaking;"),
        fixture("Font", "missing-encoding-capability", "private: using CompleteFont::supportsEncoding;"),
        fixture("Font", "runtime-baking-capability", "inline static bool supportsBaking = true;"),
        fixture("Font", "runtime-encoding-capability", "inline static bool supportsEncoding = true;"),
        fixture("Font", "wrong-capability-type", "static constexpr uint32_t supportsBaking = 1;"),
        fixture("Icon", "missing-parse", "void parse() = delete;"),
        fixture("Icon", "missing-raster", "void rasterize() = delete;"),
        fixture("Icon", "wrong-return", "bool rasterize(const Document&, const icon::IconRasterRequest&) noexcept;"),
        fixture("Icon", "throwing-parse", "Result<Document> parse(std::span<const std::byte>);"),
        fixture("Icon", "missing-const-observer", "icon::IconDimensions intrinsicSize(const Document&) noexcept;"),
        fixture("Icon", "wrong-document-observer", "icon::IconDimensions contentSize(Document&) const noexcept;"),
        fixture("Icon", "pointer-document", "using Document = void*;"),
        fixture("Icon", "copyable-document", "struct Document {};"),
        fixture("Icon", "throwing-move-document", "struct Document { Document(const Document&) = delete; Document& operator=(const Document&) = delete; Document(Document&&); Document& operator=(Document&&) noexcept; };"),
        fixture("Icon", "throwing-move-assignment-document", "struct Document { Document(const Document&) = delete; Document& operator=(const Document&) = delete; Document(Document&&) noexcept; Document& operator=(Document&&); };"),
        fixture("Icon", "polymorphic-document", "struct Document { Document(const Document&) = delete; Document& operator=(const Document&) = delete; Document(Document&&) noexcept; Document& operator=(Document&&) noexcept; virtual ~Document() noexcept; };"),
    ])
    includes = [f"-I{arguments.build / 'include'}", f"-I{arguments.source / 'include'}",
                f"-I{arguments.source / 'external'}"]
    records = []
    all_cases = [("complete", "", True, [])]
    all_cases.extend((f"complete-dev-{level}", "", True,
                      ["-DFLOW_UI_DEV_MODE=1", f"-DFLOWUI_DEV_MEMORY_LEVEL={level}"])
                     for level in range(5))
    all_cases.extend((name, body, False, []) for name, body in cases)
    for name, body, expected, definitions in all_cases:
        source = evidence / f"{name}.cpp"
        source.write_text(PREAMBLE + body)
        command = [arguments.compiler, "-std=c++23", "-fsyntax-only", *definitions, *includes, str(source)]
        result = subprocess.run(command, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
        (evidence / f"{name}.log").write_text(result.stdout)
        record = {"case": name, "exit_code": result.returncode, "expected_success": expected}
        records.append(record)
        if expected and result.returncode:
            raise AssertionError(f"Complete provider declarations rejected: {name}\n{result.stdout[-5000:]}")
        if not expected:
            marker = f"INTENDED_{name.split('-')[0].upper()}_PROVIDER_REJECTION"
            if result.returncode == 0 or marker not in result.stdout:
                raise AssertionError(f"Missing intended domain diagnostic: {name}\n{result.stdout[-5000:]}")
    (evidence / "results.json").write_text(json.dumps(records, indent=2) + "\n")
    print(f"CPU provider contracts: {len(records)} positive/negative compile cases passed.")


if __name__ == "__main__":
    main()
