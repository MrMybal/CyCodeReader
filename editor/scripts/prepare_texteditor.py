"""Extend a build-local copy of the pinned editor, leaving downloaded sources intact."""
import argparse
from pathlib import Path

def prepare(source, destination):
    if destination.resolve() == source.resolve() or destination.resolve().is_relative_to(source.resolve()):
        raise RuntimeError('Prepare the editor in a separate build directory')
    edits = {
        'TextEditor.h': [
            # CyUI already fixes exact final-newline extraction in its bundled backend.
            ('inline void SetPalette(const Palette& newPalette) { paletteBase = newPalette; paletteAlpha = -1.0f; }',
             'inline void SetPalette(const Palette& newPalette) { paletteBase = newPalette; if (paletteTransform) paletteTransform(paletteBase); paletteAlpha = -1.0f; }\n\tvoid SetPaletteTransform(std::function<void(Palette&)> callback) { paletteTransform = std::move(callback); }'),
            ('\tPalette paletteBase;', '\tPalette paletteBase;\n\tstd::function<void(Palette&)> paletteTransform;'),
            ('\tvoid SetLanguage(const Language* language);', '\tvoid SetLanguage(const Language* language);\n\tvoid SetSelections(const std::vector<DocSelection>& selections);\n\tvoid SetInputHandler(std::function<bool()> handler) { inputHandler = std::move(handler); }\n\tvoid SetResetHandler(std::function<void()> handler) { resetHandler = std::move(handler); }'),
            ('\tvoid SetLanguage(const Language* language);', '\tvoid SetLanguage(const Language* language);\n\tvoid CollapseUndoCursors(size_t firstTransaction);'),
            ('\t} colorizer;', '\t} colorizer;\n\tstd::function<bool()> inputHandler;\n\tstd::function<void()> resetHandler;'),
            ('\t\tstd::function<void(AutoCompleteState&)> callback;', '\t\tstd::function<void(AutoCompleteState&)> callback;\n\t\t// Return true when the host handles the selected item atomically.\n\t\tstd::function<bool(size_t)> accept;'),
            ('\t\tinline DocPos getStart() const { return startLocation; }', '\t\tinline bool acceptSelection() { return configuration.accept && configuration.accept(currentSelection); }\n\t\tinline DocPos getStart() const { return startLocation; }'),
            ('\t\tinline void setBeforeState(const Cursors& cursors) { before = cursors; }', '\t\tbool preserveTabs = false;\n\t\tinline void setBeforeState(const Cursors& cursors) { before = cursors; }'),
            ('\tvoid SetLanguage(const Language* language);', '\tvoid ReplaceTextPreservingTabs(const std::string_view& text, bool allCursors=false);\n\tvoid SetLanguage(const Language* language);'),
            ('\t\tcurrentLineHighlightBorder,\n\t\tcount', '\t\tcurrentLineHighlightBorder,\n\t\tfunction,\n\t\tmacro,\n\t\tattribute,\n\t\tinactive,\n\t\tfield,\n\t\tparameter,\n\t\tenumerator,\n\t\tcount'),
            ('\t\tColor color = Color::text;', '\t\tColor color = Color::text;\n\t\tColor semanticColor = Color::count;\n\t\tColor syntaxColor = Color::count;'),
            ('\t} colorizer;', '\t} colorizer;\n\tsize_t semanticVersion = ~size_t(0), syntaxVersion = ~size_t(0);'),
            ('\t\tinline Iterator& operator++() { glyph++; return *this; }', '\t\tinline Iterator& operator++() { glyph++; return *this; }\n\t\tinline Iterator& operator--() { glyph--; return *this; }'),
            ('\tvoid SetLanguage(const Language* language);', '\tvoid SetLanguage(const Language* language);\n\tvoid ClearSyntaxTokens() { syntaxVersion = ~size_t(0); }'),
            ('\t\tstd::function<Iterator(Iterator start, Iterator end, Color& color)> customTokenizer;',
             '\t\tstd::function<Iterator(Iterator start, Iterator end, Color& color)> customTokenizer;\n\t\t// CyCode: bounded context-aware lexical highlighting, after strings/comments.\n\t\tstd::function<Iterator(Iterator lineStart, Iterator start, Iterator end, Color& color)> contextTokenizer;'),
            ('\tvoid SetLanguage(const Language* language);', '\tvoid SetLanguage(const Language* language);\n\t// CyCode: lexical state stays separate from display-only semantic overlays.\n\tColor GetTokenColor(DocPos position);\n\tColor GetDisplayColor(DocPos position);\n\tstruct SemanticToken { size_t line, start, end; Color color; };\n\tvoid SetSemanticTokens(const std::vector<SemanticToken>& tokens);\n\tvoid ClearSemanticTokens() { semanticVersion = ~size_t(0); }'),
        ],
        'TextEditor.cpp': [
            ('void TextEditor::handleKeyboardInputs() {\n\tif (ImGui::IsWindowFocused()) {', 'void TextEditor::handleKeyboardInputs() {\n\tif (ImGui::IsWindowFocused()) {\n\t\tif (inputHandler && inputHandler()) return;'),
            ('\tif (autocomplete.render(document, cursors, typeSetter, config.language, textLeftOffset, glyphSize)) {', '\tif (autocomplete.render(document, cursors, typeSetter, config.language, textLeftOffset, glyphSize) && !autocomplete.acceptSelection()) {'),
            ('\tconst auto transaction = at(--undoIndex);', '\tconst auto transaction = at(--undoIndex);\n\tauto exact = config; exact.insertSpacesOnTabs = false;'),
            ('\tconst auto transaction = at(undoIndex++);', '\tconst auto transaction = at(undoIndex++);\n\tauto exact = config; exact.insertSpacesOnTabs = false;'),
            ('\t\t} else {\n\t\t\tdocument.insertText(config, action->start, action->text);', '\t\t} else {\n\t\t\tdocument.insertText(transaction->preserveTabs ? exact : config, action->start, action->text);'),
            ('\t\tif (action->type == Action::Type::insertText) {\n\t\t\tdocument.insertText(config, action->start, action->text);', '\t\tif (action->type == Action::Type::insertText) {\n\t\t\tdocument.insertText(transaction->preserveTabs ? exact : config, action->start, action->text);'),
            ('#include <set>', '#include <set>\n#include <stdexcept>'),
            ('void TextEditor::reset() {', 'void TextEditor::reset() {\n\tif (resetHandler) resetHandler();\n\tClearSemanticTokens(); ClearSyntaxTokens();'),
            ('void TextEditor::SetLanguage(const Language* language) {', 'void TextEditor::SetLanguage(const Language* language) {\n\tClearSemanticTokens(); ClearSyntaxTokens();'),
            ('palette.get(glyph.color), codepoint);', 'palette.get(semanticVersion == transactions.getVersion() && glyph.semanticColor != Color::count ? glyph.semanticColor : syntaxVersion == transactions.getVersion() && glyph.syntaxColor != Color::count ? glyph.syntaxColor : glyph.color), codepoint);'),
            ('\t\tcolor == TextEditor::Color::knownIdentifier;',
             '\t\tcolor == TextEditor::Color::knownIdentifier ||\n\t\tcolor == TextEditor::Color::declaration ||\n\t\tcolor == TextEditor::Color::function ||\n\t\tcolor == TextEditor::Color::macro ||\n\t\tcolor == TextEditor::Color::attribute;'),
            ('state.inIdentifier = color == Color::identifier || color == Color::knownIdentifier;',
             'state.inIdentifier = isIdentifier(color);'),
            ('\t\t\t\t// handle custom tokenizer (if we have one)\n\t\t\t\tif (language->customTokenizer',
             '\t\t\t\t// CyCode context tokenizer preserves the built-in comment/string state machine.\n\t\t\t\tif (language->contextTokenizer && (tokenEnd = language->contextTokenizer(Iterator(line.data()), tokenStart, lineEnd, color)) != tokenStart) {\n\t\t\t\t\tconst auto size = tokenEnd - tokenStart;\n\t\t\t\t\tsetColor(glyph, glyph + size, color); glyph += size;\n\t\t\t\t} else if (language->customTokenizer'),
        ],
    }
    shared = 'SetSharedSyntaxTokens' in (source/'TextEditor.h').read_text('utf-8')
    if shared:
        import re
        header = (source/'TextEditor.h').read_text('utf-8')
        tail = re.search(r'(\t\t[^\n]+sharedAnsiWhite,)\n\t\tcount', header)
        if not tail: raise RuntimeError('Shared CyUI editor palette anchor changed')
        for i, entry in enumerate(edits['TextEditor.h']):
            if entry[0] == '\t\tcurrentLineHighlightBorder,\n\t\tcount':
                before=tail[0]
                after=entry[1].replace('\t\tcurrentLineHighlightBorder,',tail[1])
                edits['TextEditor.h'][i]=(before,after)
        fallback='sharedSyntaxVersion == transactions.getVersion() && glyph.sharedSyntaxColor != Color::count ? glyph.sharedSyntaxColor : glyph.color'
        for i, entry in enumerate(edits['TextEditor.cpp']):
            if entry[0] == 'palette.get(glyph.color), codepoint);':
                edits['TextEditor.cpp'][i]=('palette.get('+fallback+'), codepoint);',entry[1].replace(': glyph.color),',': '+fallback+'),'))
        edits['TextEditor.cpp']=[entry for entry in edits['TextEditor.cpp'] if entry[0]!='#include <set>']

    # Display-only Reader search spans. This modifies the one build-local backend,
    # never CyUI's downloaded source, document markers, cursors or Undo history.
    edits['TextEditor.h'] += [
        ('\tinline void SetAutoCompleteConfig(const AutoCompleteConfig* autoCompleteConfig) { autocomplete.setConfig(autoCompleteConfig); }',
         '\tinline void SetAutoCompleteConfig(const AutoCompleteConfig* autoCompleteConfig) { autocomplete.setConfig(autoCompleteConfig); }\n\tImGuiKeyChord GetAutoCompleteShortcut() const { return autocomplete.getTriggerShortcut(); }'),
        ('\tvoid SetLanguage(const Language* language);', '\tvoid SetLanguage(const Language* language);\n\tvoid SetReaderSearchMatches(std::vector<DocSelection> ranges, ImU32 color) { readerSearch=std::move(ranges);readerSearchColor=color;readerSearchVersion=transactions.getVersion(); }\n\tsize_t GetReaderSearchMatchCount() const { return readerSearchVersion==transactions.getVersion() ? readerSearch.size() : 0; }'),
        ('\tvoid renderSelections();','\tvoid renderSelections();\n\tvoid renderReaderSearch();\n\tstd::vector<DocSelection> readerSearch;\n\tImU32 readerSearchColor=0;\n\tsize_t readerSearchVersion=~size_t(0);')
    ]
    edits['TextEditor.cpp'] += [
        ('\t\trenderSelections();','\t\trenderReaderSearch();\n\t\trenderSelections();'),
        ('\t\trenderScrollbarMiniMap();','\t\trenderScrollbarMiniMap();\n\t\tif (readerSearchVersion==transactions.getVersion()) {\n\t\t\tconst auto origin=ImGui::GetWindowPos();\n\t\t\tconst auto window=ImGui::GetCurrentWindow();\n\t\t\tconst auto rail=ImGui::GetWindowScrollbarRect(window,ImGuiAxis_Y);\n\t\t\tfor (const auto &match : readerSearch) {\n\t\t\t\tif (match.start.line>=document.size()) continue;\n\t\t\t\tconst auto row=document[match.start.line].row;\n\t\t\t\tif (config.showMiniMap && row>=firstMiniMapRow && row<lastMiniMapRow) {\n\t\t\t\t\tconst auto y=origin.y+(row-firstMiniMapRow)*miniMapRowHeight;\n\t\t\t\t\tdrawList->AddRectFilled(ImVec2(origin.x+miniMapOffset,y),ImVec2(origin.x+miniMapOffset+miniMapWidth,y+std::max(1.f,miniMapRowHeight*.65f)),readerSearchColor);\n\t\t\t\t}\n\t\t\t\tif (window->ScrollbarY) {\n\t\t\t\t\tconst float y=rail.Min.y+rail.GetHeight()*float(row)/float(std::max(size_t(1),typeSetter.getRowCount()));\n\t\t\t\t\tdrawList->PushClipRect(rail.Min,rail.Max,false);drawList->AddRectFilled(ImVec2(rail.Min.x,y),ImVec2(rail.Max.x,y+std::max(2.f,fontScaleDpi)),readerSearchColor);drawList->PopClipRect();\n\t\t\t\t}\n\t\t\t}\n\t\t}')
    ]

    # Older published CyUI pins keep the compatibility path; new CyUI owns it.
    if "void SetFontScale(float value);" not in (source/"TextEditor.h").read_text("utf-8"):
        # Host font/zoom and scope guides are compatibility patches to the same
        # build-local backend. Never mutate the locked CyUI checkout.
        edits['TextEditor.h'] += [
            ('\tvoid renderSelections();', '\tvoid renderSelections();\n\tImFont *readerFont=nullptr;\n\tfloat readerTextScale=1.f;'),
            ('\tvoid SetLanguage(const Language* language);', '\tvoid SetLanguage(const Language* language);\n\tvoid SetReaderFont(ImFont *value) { readerFont=value; }\n\tfloat GetReaderTextScale() const { return readerTextScale; }')
        ]
        edits['TextEditor.cpp'] += [
            ('\t\t// get font information', '''\t\t// Ctrl+wheel belongs only to the hovered text surface, including previews.
            auto &readerIo=ImGui::GetIO();
            const auto previousScale=readerTextScale;
            if (ImGui::IsWindowHovered() && readerIo.KeyCtrl && !readerIo.KeyAlt && !readerIo.KeyShift && readerIo.MouseWheel!=0.f) {
                readerTextScale=std::clamp(readerTextScale+readerIo.MouseWheel*.1f,.5f,3.f);
                ImGui::SetKeyOwner(ImGuiKey_MouseWheelY,ImGui::GetCurrentWindow()->ID);
            }
            const auto previousHeight=glyphSize.y;
            ImGui::PushFont(readerFont,ImGui::GetStyle().FontSizeBase*readerTextScale);
            // get font information'''),
            ('\t\t// determine current position and visible size', '''\t\tif (previousScale!=readerTextScale && previousHeight>0.f)
                ImGui::SetScrollY(ImGui::GetScrollY()*glyphSize.y/previousHeight);
            // determine current position and visible size'''),
            ('\t}\n\n\tImGui::EndChild();', '\t\tImGui::PopFont();\n\t}\n\n\tImGui::EndChild();'),
            ('if (bracket->visible && bracket->end.line - bracket->start.line > 1) {',
             "if (bracket->visible && bracket->end.line - bracket->start.line > 1 && document[bracket->start.line][bracket->start.index].codepoint == '{') {"),
        ]

    destination.mkdir(parents=True, exist_ok=True)
    prepared = {}
    for name, replacements in edits.items():
        content = (source/name).read_text('utf-8')
        for replacement in replacements:
            before,after=replacement[:2]
            expected=replacement[2] if len(replacement)==3 else 1
            if content.count(before) != expected:
                raise RuntimeError(f'Pinned editor patch anchor changed: {name}: {before[:65]!r}')
            content = content.replace(before, after)
        if name == 'TextEditor.cpp':
            content += '''
void TextEditor::ReplaceTextPreservingTabs(const std::string_view& text, bool allCursors) {
    if (config.readOnly) return;
    const bool policy=config.insertSpacesOnTabs;
    const auto version=transactions.getVersion();
    config.insertSpacesOnTabs=false;
    try { if (allCursors) replaceTextInAllCursors(text); else replaceTextInCurrentCursor(text); }
    catch (...) { config.insertSpacesOnTabs=policy; throw; }
    config.insertSpacesOnTabs=policy;
    if (version!=transactions.getVersion() && !transactions.empty()) transactions.back()->preserveTabs=true;
}
void TextEditor::SetSelections(const std::vector<DocSelection>& selections) {
    if (selections.empty() || selections.size()>128) throw std::runtime_error("Invalid selection count");
    for (size_t i=0;i<selections.size();++i) {
        const auto& s=selections[i];
        if (s.end<s.start || normalizePos(s.start)!=s.start || normalizePos(s.end)!=s.end ||
            (i && (s.start<selections[i-1].end || (s.start==selections[i-1].start && s.end==selections[i-1].end)))) throw std::runtime_error("Invalid or overlapping selections");
    }
    autocomplete.cancel();
    setCursor(selections[0].end);cursors.setCursor(selections[0].start,selections[0].end);
    for (size_t i=1;i<selections.size();++i) cursors.addCursor(selections[i].start,selections[i].end);
}
void TextEditor::CollapseUndoCursors(size_t firstTransaction) {
    for (size_t i=firstTransaction;i<transactions.size();++i) {
        auto& t=transactions[i];auto before=t->getBeforeState(),after=t->getAfterState();
        before.clearAdditional();after.clearAdditional();t->setBeforeState(before);t->setAfterState(after);
    }
}
TextEditor::Color TextEditor::GetTokenColor(DocPos position) {
    colorizer.update(config, document);
    if (position.line >= document.size() || position.index >= document[position.line].size()) return Color::text;
    const auto &glyph=document[position.line][position.index];
    return syntaxVersion == transactions.getVersion() && glyph.syntaxColor != Color::count ? glyph.syntaxColor : glyph.color;
}
TextEditor::Color TextEditor::GetDisplayColor(DocPos position) {
    const auto lexical = GetTokenColor(position);
    if (position.line >= document.size() || position.index >= document[position.line].size()) return lexical;
    const auto semantic = document[position.line][position.index].semanticColor;
    return semanticVersion == transactions.getVersion() && semantic != Color::count ? semantic : lexical;
}
void TextEditor::SetSemanticTokens(const std::vector<SemanticToken>& tokens) {
    if (tokens.size() > 50000) throw std::runtime_error("Too many semantic display spans");
    size_t previousLine=0, previousEnd=0; bool first=true;
    for (const auto &token : tokens) {
        if (token.line >= document.size() || token.start >= token.end || token.end > document[token.line].size() ||
            token.color < Color::text || token.color >= Color::count ||
            (!first && (token.line < previousLine || (token.line == previousLine && token.start < previousEnd))))
            throw std::runtime_error("Invalid semantic display span");
        previousLine=token.line; previousEnd=token.end; first=false;
    }
    colorizer.update(config, document);
    for (auto &line : document) for (auto &glyph : line) glyph.semanticColor=Color::count;
    for (const auto &token : tokens) for (size_t i=token.start;i<token.end;++i) {
        auto &glyph=document[token.line][i];
        const auto lexical=syntaxVersion == transactions.getVersion() && glyph.syntaxColor != Color::count ? glyph.syntaxColor : glyph.color;
        // Comments/strings retain their shared lexical color, including in inactive code.
        if (lexical == Color::comment || lexical == Color::string) continue;
        if (token.color == Color::declaration && lexical == Color::keyword) continue;
        glyph.semanticColor=token.color;
    }
    semanticVersion=transactions.getVersion();
}
void TextEditor::SetSyntaxTokens(const std::vector<SemanticToken>& tokens) {
    if (tokens.size()>200001) throw std::runtime_error("Too many syntax spans");
    size_t line=0,end=0; bool first=true;
    for (const auto &token : tokens) {
        if (token.line>=document.size() || token.start>=token.end || token.end>document[token.line].size() ||
            token.color<Color::text || token.color>=Color::count ||
            (!first && (token.line<line || (token.line==line && token.start<end)))) throw std::runtime_error("Invalid syntax span");
        line=token.line; end=token.end; first=false;
    }
    for (auto &line : document) for (auto &glyph : line) glyph.syntaxColor=Color::count;
    for (const auto &token : tokens) for (size_t i=token.start;i<token.end;++i) document[token.line][i].syntaxColor=token.color;
    syntaxVersion=transactions.getVersion();
}
'''

        if name == 'TextEditor.cpp':
            # Reuse the backend's folding/wrapping-aware selection geometry.
            original=content[content.index('void TextEditor::renderSelections() {'):content.index('//\n//\tTextEditor::renderTextMarkers')]
            original=original[:original.rfind('}')+1]
            overlay=original.replace('renderSelections()', 'renderReaderSearch()').replace('for (const auto& cursor : cursors)', 'if (readerSearchVersion!=transactions.getVersion()) return;\n\tfor (const auto& cursor : readerSearch)')
            overlay=overlay.replace('cursor.hasSelection()', 'cursor.start!=cursor.end').replace('cursor.getSelectionStart()', 'cursor.start').replace('cursor.getSelectionEnd()', 'cursor.end')
            overlay=overlay.replace('palette.get(Color::selection)', 'ImGui::GetColorU32(readerSearchColor, .28f)')
            content+='\n'+overlay+'\n'

        if name == 'TextEditor.h':
            content = content.replace('void SetSemanticTokens(const std::vector<SemanticToken>& tokens);', 'void SetSemanticTokens(const std::vector<SemanticToken>& tokens);\n\tvoid SetSyntaxTokens(const std::vector<SemanticToken>& tokens);')
        if shared and name == 'TextEditor.cpp':
            content=content.replace('syntaxVersion == transactions.getVersion() && glyph.syntaxColor != Color::count ? glyph.syntaxColor : glyph.color;',
                                    'syntaxVersion == transactions.getVersion() && glyph.syntaxColor != Color::count ? glyph.syntaxColor : GetSharedSyntaxColor(position);',1)
        if shared and name == 'TextEditor.cpp':
            content=content.replace('syntaxVersion == transactions.getVersion() && glyph.syntaxColor != Color::count ? glyph.syntaxColor : glyph.color;',
                                    'syntaxVersion == transactions.getVersion() && glyph.syntaxColor != Color::count ? glyph.syntaxColor : GetSharedSyntaxColor({token.line,i});')
        prepared[name] = content

    from text_assistance import augment
    prepared['TextEditor.h'], prepared['TextEditor.cpp'] = augment(prepared['TextEditor.h'], prepared['TextEditor.cpp'])
    for name, content in prepared.items():
        path = destination/name
        data = content.encode('utf-8')
        if not path.exists() or path.read_bytes() != data:
            path.write_bytes(data)

if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source', type=Path, required=True)
    parser.add_argument('--destination', type=Path, required=True)
    args = parser.parse_args()
    prepare(args.source, args.destination)
