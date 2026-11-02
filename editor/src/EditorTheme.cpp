#include "cycode/EditorTheme.h"
#include <algorithm>
#include <cmath>
static_assert(IMGUI_VERSION_NUM >= 19200, "CyUI requires Dear ImGui 1.92 or newer");
namespace cycode {
ImU32 readableText(ImU32 foreground, std::initializer_list<ImU32> backgrounds) {
    auto luminance = [](ImU32 color) {
        auto c = ImGui::ColorConvertU32ToFloat4(color);
        auto linear = [](float n) { return n <= .04045f ? n / 12.92f : std::pow((n + .055f) / 1.055f, 2.4f); };
        return .2126f*linear(c.x) + .7152f*linear(c.y) + .0722f*linear(c.z);
    };
    auto minimumContrast = [&](ImU32 color) {
        const auto f = luminance(color); float ratio = 100.f;
        for (auto background : backgrounds) { const auto b = luminance(background); ratio = std::min(ratio,(std::max(f,b)+.05f)/(std::min(f,b)+.05f)); }
        return ratio;
    };
    if (minimumContrast(foreground) >= 4.5f) return foreground;
    const auto &p = cyui::palette(); auto target = p.text;
    for (auto candidate : {p.text,p.accentText,p.bg0}) if (minimumContrast(candidate) > minimumContrast(target)) target = candidate;
    for (int step = 1; step <= 20; ++step) { auto color = cyui::mix(foreground,target,step/20.f); if (minimumContrast(color) >= 4.5f) return color; }
    return target;
}
void applyEditorTheme(const std::string &key) {
    if (key != "graphite") {
        cyui::setTheme(key.c_str());
        return;
    }
    // CyCode's neutral palette, inspired by the user's Rider reference.
    cyui::setTheme("navy"); // Also reset CyUI's light/dark modal treatment.
    auto p = cyui::palette();
    p.bg0 = cyui::rgb(0x242424);
    p.bg1 = cyui::rgb(0x2b2d30);
    p.bg2 = cyui::rgb(0x303236);
    p.bg3 = cyui::rgb(0x35383e);
    p.bg4 = cyui::rgb(0x41454c);
    p.bg5 = cyui::rgb(0x4b5059);
    p.line = cyui::rgb(0x343638);
    p.lineStrong = cyui::rgb(0x1e2022);
    p.grid = cyui::rgb(0x303236);
    p.gridStrong = cyui::rgb(0x3b3e42);
    p.text = cyui::rgb(0xbcbec4);
    p.textDim = cyui::rgb(0x9da0a8);
    p.textFaint = cyui::rgb(0x7c818c);
    p.accent = cyui::rgb(0x4b83f5);
    p.accentHover = cyui::rgb(0x679bff);
    p.accentDim = cyui::rgb(0x33486e);
    p.accentText = cyui::rgb(0xffffff);
    p.selection = cyui::rgb(0x30456d);
    p.success = cyui::rgb(0x6aab73);
    p.warning = cyui::rgb(0xd5b778);
    p.error = cyui::rgb(0xf07070);
    p.info = cyui::rgb(0x7e9ac3);
    cyui::setPalette(p);
}
SyntaxColours syntaxColours(const std::string &) {
    // One common token palette for the editor, source views and Tree-sitter overlays.
    using R=cyui::TokenRole;
    return {cyui::tokenColor(R::keyword),cyui::tokenColor(R::type),cyui::tokenColor(R::number),
        cyui::tokenColor(R::string),cyui::tokenColor(R::comment),cyui::tokenColor(R::preprocessor),
        cyui::tokenColor(R::function),cyui::tokenColor(R::preprocessor),cyui::tokenColor(R::type)};
}
void configureCodeEditor(cyui::CodeEditor &editor) {
    editor.native().SetPaletteTransform([](TextEditor::Palette &colors) {
#ifdef CYUI_CODE_LANGUAGE_CATALOGUE
        const auto shared=colors;
#endif
        colors=editorColours("");
#ifdef CYUI_CODE_LANGUAGE_CATALOGUE
        for (auto i=size_t(TextEditor::Color::sharedFunction);i<=size_t(TextEditor::Color::sharedAnsiWhite);++i) colors[i]=shared[i];
#endif
    });
}
TextEditor::Palette editorColours(const std::string &key) {
    const auto &p=cyui::palette(); const auto syntax=syntaxColours(key);
    const std::array<ImU32,18> inputs={p.bg0,p.bg1,p.text,p.textFaint,p.accent,p.selection,p.lineStrong,p.error,p.accentText,
        syntax.keyword,syntax.type,syntax.number,syntax.string,syntax.comment,syntax.preprocessor,syntax.function,syntax.macro,syntax.attribute};
    static std::array<ImU32,18> last{}; static TextEditor::Palette cached; static bool initialized=false;
    if (initialized && inputs==last) return cached;
    TextEditor::Palette result; result.fill(p.text);
    auto set=[&](TextEditor::Color role,ImU32 colour) { result[size_t(role)]=colour; };
    set(TextEditor::Color::background,p.bg0);
    set(TextEditor::Color::keyword,syntax.keyword);
    set(TextEditor::Color::declaration,syntax.type);
    set(TextEditor::Color::number,syntax.number);
    set(TextEditor::Color::string,syntax.string);
    set(TextEditor::Color::comment,syntax.comment);
    set(TextEditor::Color::preprocessor,syntax.preprocessor);
    set(TextEditor::Color::function,syntax.function);
    set(TextEditor::Color::macro,syntax.macro);
    set(TextEditor::Color::attribute,syntax.attribute);
    set(TextEditor::Color::knownIdentifier,syntax.macro);
    set(TextEditor::Color::inactive,p.textFaint);
    set(TextEditor::Color::parameter,p.text);
    set(TextEditor::Color::field,p.text);
    set(TextEditor::Color::enumerator,syntax.type);
    set(TextEditor::Color::selection,p.selection);
    set(TextEditor::Color::cursor,p.text);
    set(TextEditor::Color::lineNumber,p.textFaint);
    set(TextEditor::Color::currentLineNumber,p.accent);
    set(TextEditor::Color::currentLineHighlight,p.bg1);
    set(TextEditor::Color::currentLineHighlightBorder,p.bg1);
    set(TextEditor::Color::whitespace,p.lineStrong);
    set(TextEditor::Color::matchingBracketBackground,p.selection);
    set(TextEditor::Color::matchingBracketError,p.error);
    for (auto role : {TextEditor::Color::text,TextEditor::Color::identifier,TextEditor::Color::punctuation,
        TextEditor::Color::keyword,TextEditor::Color::declaration,TextEditor::Color::number,TextEditor::Color::string,
        TextEditor::Color::comment,TextEditor::Color::preprocessor,TextEditor::Color::function,TextEditor::Color::macro,
        TextEditor::Color::attribute,TextEditor::Color::knownIdentifier,TextEditor::Color::inactive,TextEditor::Color::parameter,
        TextEditor::Color::field,TextEditor::Color::enumerator,TextEditor::Color::lineNumber,TextEditor::Color::currentLineNumber}) {
        auto &colour=result[size_t(role)]; colour=readableText(colour,{p.bg0,p.bg1});
    }
    cached=result; last=inputs; initialized=true; return result;
}
} // namespace cycode
