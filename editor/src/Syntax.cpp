#include "cycode/Syntax.h"
#include <tree_sitter/api.h>
#include <algorithm>
#include <chrono>
#include <cstring>
#include <stdexcept>
#include <string_view>
#include <vector>
extern "C" {
const TSLanguage *tree_sitter_rust();
const TSLanguage *tree_sitter_java();
const TSLanguage *tree_sitter_javascript();
const TSLanguage *tree_sitter_typescript();
const TSLanguage *tree_sitter_tsx();
const TSLanguage *tree_sitter_html();
const TSLanguage *tree_sitter_css();
}
namespace cycode {
namespace {
using Color=TextEditor::Color;
const TSLanguage *grammar(const std::string &language) {
    if (language=="rust") return tree_sitter_rust();
    if (language=="java") return tree_sitter_java();
    if (language=="javascript") return tree_sitter_javascript();
    if (language=="typescript") return tree_sitter_typescript();
    if (language=="tsx") return tree_sitter_tsx();
    if (language=="html") return tree_sitter_html();
    if (language=="css") return tree_sitter_css();
    return nullptr;
}
bool oneOf(std::string_view value,std::initializer_list<std::string_view> options) { return std::find(options.begin(),options.end(),value)!=options.end(); }
TSPoint point(const std::string &text,size_t end) {
    TSPoint result{};
    for (size_t i=0;i<end;++i) { if (text[i]=='\n') { ++result.row; result.column=0; } else ++result.column; }
    return result;
}
Color role(TSNode node,std::string_view parent,const char *field,bool &terminal) {
    const std::string_view kind=ts_node_type(node);
    terminal=false;
    if (kind.find("comment")!=kind.npos) { terminal=true; return Color::comment; }
    if (oneOf(kind,{"string","string_literal","raw_string_literal","character_literal","char_literal","regex","text_block","quoted_attribute_value"})) { terminal=true; return Color::string; }
    if (oneOf(kind,{"template_string","string_fragment","string_content","escape_sequence","jsx_text","attribute_value"})) return Color::string;
    if (kind=="template_substitution") return Color::identifier;
    if (kind.find("literal")!=kind.npos && (kind.find("integer")!=kind.npos || kind.find("floating")!=kind.npos)) { terminal=true; return Color::number; }
    if (oneOf(kind,{"integer_literal","float_literal","number","integer_value","float_value","color_value"})) { terminal=true; return Color::number; }
    if (oneOf(kind,{"primitive_type","predefined_type","integral_type","floating_point_type","boolean_type","void_type"})) { terminal=true; return Color::keyword; }
    if (oneOf(kind,{"type_identifier","namespace_identifier","tag_name","class_name","id_name"})) { terminal=true; return Color::declaration; }
    if (oneOf(kind,{"property_name","attribute_name","lifetime","decorator","annotation","marker_annotation"})) return Color::attribute;
    if (oneOf(kind,{"property_identifier","field_identifier","shorthand_property_identifier_pattern"})) return Color::field;
    if (kind=="function_name") return Color::function;
    if (kind=="doctype") { terminal=true; return Color::preprocessor; }
    if (kind=="identifier" || kind=="shorthand_property_identifier") {
        const std::string_view name=field ? field : "";
        if ((parent=="macro_invocation" && name=="macro") || parent=="macro_definition") return Color::macro;
        if ((name=="name" && oneOf(parent,{"function_item","function_declaration","function_expression","generator_function_declaration","method_declaration","method_definition","constructor_declaration","method_invocation"})) ||
            (name=="function" && parent=="call_expression")) return Color::function;
        if (name=="name" && oneOf(parent,{"class_declaration","interface_declaration","enum_declaration","record_declaration","type_alias_declaration","struct_item","enum_item","trait_item","type_item"})) return Color::declaration;
        if (oneOf(parent,{"annotation","marker_annotation","jsx_opening_element","jsx_closing_element","jsx_self_closing_element"}) && name=="name") return Color::declaration;
        return Color::identifier;
    }
    if (!ts_node_is_named(node)) {
        if (kind.size() && ((kind.front()>='a' && kind.front()<='z') || kind=="Self")) return Color::keyword;
        return Color::punctuation;
    }
    return Color::count;
}
struct Painter {
    const std::string &text;
    std::vector<Color> colors;
    std::chrono::steady_clock::time_point started=std::chrono::steady_clock::now();
    size_t nodes=0;
    bool truncated=false;
    explicit Painter(const std::string &text) : text(text),colors(text.size(),Color::identifier) {}
    void walk(TSNode root,size_t offset=0,int injection=0) {
        auto cursor=ts_tree_cursor_new(root);
        bool entering=true;
        for (;;) {
            bool descend=false;
            if (entering) {
                if (++nodes>1000000 || ((nodes&1023)==0 && std::chrono::steady_clock::now()-started>std::chrono::milliseconds(50))) { truncated=true; break; }
                const auto node=ts_tree_cursor_current_node(&cursor);
                const auto parent=ts_node_parent(node);
                const std::string_view parentKind=ts_node_is_null(parent) ? "" : ts_node_type(parent);
                const auto field=ts_tree_cursor_current_field_name(&cursor);
                const auto start=std::min(offset+ts_node_start_byte(node),text.size());
                const auto end=std::min(offset+ts_node_end_byte(node),text.size());
                if (std::string_view(ts_node_type(node))=="raw_text" && injection==0 && oneOf(parentKind,{"script_element","style_element"})) {
                    auto parser=ts_parser_new(); ts_parser_set_language(parser,parentKind=="style_element" ? tree_sitter_css() : tree_sitter_javascript());
                    ts_parser_set_timeout_micros(parser,20000);
                    if (auto tree=ts_parser_parse_string(parser,nullptr,text.data()+start,static_cast<uint32_t>(end-start))) { walk(ts_tree_root_node(tree),start,1); ts_tree_delete(tree); }
                    else truncated=true;
                    ts_parser_delete(parser);
                } else {
                    bool terminal=false;
                    const auto color=role(node,parentKind,field,terminal);
                    if (color!=Color::count && start<end) std::fill(colors.begin()+start,colors.begin()+end,color);
                    descend=!terminal;
                }
            }
            if (entering && descend && ts_tree_cursor_goto_first_child(&cursor)) { entering=true; continue; }
            if (ts_tree_cursor_goto_next_sibling(&cursor)) { entering=true; continue; }
            if (!ts_tree_cursor_goto_parent(&cursor)) break;
            entering=false;
        }
        ts_tree_cursor_delete(&cursor);
    }
};
}
struct Syntax::State {
    TSParser *parser=ts_parser_new();
    TSTree *tree=nullptr;
    std::string language,text;
    size_t spans=0;
    bool truncated=false;
    ~State() { if (tree) ts_tree_delete(tree); ts_parser_delete(parser); }
};
Syntax::Syntax() : state_(std::make_unique<State>()) { ts_parser_set_timeout_micros(state_->parser,50000); }
Syntax::~Syntax()=default;
size_t Syntax::spans() const { return state_->spans; }
bool Syntax::truncated() const { return state_->truncated; }
std::string Syntax::language(const std::string &path) {
    auto dot=path.find_last_of('.'); auto ext=dot==std::string::npos ? "" : path.substr(dot);
    std::transform(ext.begin(),ext.end(),ext.begin(),[](unsigned char c) { return char(std::tolower(c)); });
    if (ext==".rs") return "rust";
    if (ext==".java") return "java";
    if (oneOf(ext,{".js",".mjs",".cjs",".jsx"})) return "javascript";
    if (oneOf(ext,{".ts",".mts",".cts"})) return "typescript";
    if (ext==".tsx") return "tsx";
    if (ext==".html" || ext==".htm") return "html";
    if (ext==".css") return "css";
    return "";
}
bool Syntax::update(TextEditor &editor,const std::string &path,const std::string &text) {
    auto &state=*state_; const auto name=language(path); const auto lang=grammar(name);
    state.spans=0; state.truncated=false;
    if (!lang) { editor.ClearSyntaxTokens(); return false; }
    if (text.size()>4*1024*1024) { editor.ClearSyntaxTokens(); state.truncated=true; return false; }
    if (name!=state.language) {
        if (!ts_parser_set_language(state.parser,lang)) throw std::runtime_error("Syntax grammar ABI mismatch");
        if (state.tree) ts_tree_delete(state.tree); state.tree=nullptr; state.text.clear(); state.language=name;
    }
    if (state.tree) {
        size_t start=0,oldEnd=state.text.size(),newEnd=text.size();
        while (start<std::min(oldEnd,newEnd) && state.text[start]==text[start]) ++start;
        while (start && start<text.size() && (static_cast<unsigned char>(text[start])&0xc0)==0x80) --start;
        while (oldEnd>start && newEnd>start && state.text[oldEnd-1]==text[newEnd-1]) { --oldEnd; --newEnd; }
        while (newEnd<text.size() && (static_cast<unsigned char>(text[newEnd])&0xc0)==0x80) { ++oldEnd; ++newEnd; }
        const TSInputEdit edit{static_cast<uint32_t>(start),static_cast<uint32_t>(oldEnd),static_cast<uint32_t>(newEnd),point(state.text,start),point(state.text,oldEnd),point(text,newEnd)};
        ts_tree_edit(state.tree,&edit);
    }
    auto next=ts_parser_parse_string(state.parser,state.tree,text.data(),static_cast<uint32_t>(text.size()));
    if (state.tree) ts_tree_delete(state.tree); state.tree=next; state.text=text;
    if (!next) { ts_parser_reset(state.parser); state.truncated=true; editor.ClearSyntaxTokens(); return false; }
    Painter painter(text); painter.walk(ts_tree_root_node(next));
    std::vector<TextEditor::SemanticToken> spans;
    size_t line=0,glyph=0,start=0; Color previous=Color::count;
    auto finish=[&] { if (previous!=Color::count && glyph>start) spans.push_back({line,start,glyph,previous}); };
    for (size_t byte=0;byte<text.size();) {
        if (spans.size()>=200000) { painter.truncated=true; break; }
        if (text[byte]=='\n') { finish(); ++line; glyph=start=0; previous=Color::count; ++byte; continue; }
        auto color=painter.colors[byte];
        if (color!=previous) { finish(); start=glyph; previous=color; }
        const auto c=static_cast<unsigned char>(text[byte]); byte+=c<0x80 ? 1 : c<0xe0 ? 2 : c<0xf0 ? 3 : 4; ++glyph;
    }
    finish();
    state.spans=spans.size(); state.truncated=painter.truncated; editor.SetSyntaxTokens(spans); return true;
}
} // namespace cycode
