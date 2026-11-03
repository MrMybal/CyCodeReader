#include "DiffView.h"
#include "Document.h"
namespace cycode::reader {
cyui::DiffDocument makeDiffDocument(const std::string &before,const std::string &after,cyui::CodeLanguage language) {
    for (const auto *text : {&before,&after}) {
        validateEditorText(*text);size_t start=0,lines=0;
        do {
            const auto end=text->find('\n',start);
            if ((end==std::string::npos ? text->size() : end)-start>65536) throw std::runtime_error("Comparison lines exceed 65536 bytes");
            if (++lines>100000) throw std::runtime_error("Comparison supports up to 100000 lines per side");
            if (end==std::string::npos) break;start=end+1;
        } while (start<text->size());
    }
    cyui::DiffDocument document;document.setTexts(before,after,language,1000000);return document;
}
}
