#include "Reader.h"
#include "Assistant.h"
#include "cycode/Paths.h"
#include "cycode/TextFile.h"
#include <imgui_stdlib.h>
namespace cycode::reader {
namespace {
TextEditor::DocPos utf16Position(TextEditor &editor,size_t line,size_t character){
    const auto text=editor.GetText();size_t at=0;
    for(size_t n=0;n<line;++n){at=text.find('\n',at);if(at==std::string::npos)throw cymcp::Error("InvalidParameters","Line is outside the document");++at;}
    size_t units=0,glyph=0;
    while(units<character&&at<text.size()&&text[at]!='\n'){
        const auto c=static_cast<unsigned char>(text[at]);const size_t bytes=c>=0xf0?4:c>=0xe0?3:c>=0xc0?2:1;
        units+=bytes==4?2:1;at+=bytes;++glyph;
    }
    if(units!=character)throw cymcp::Error("InvalidParameters","Invalid UTF-16 caret position");return {line,glyph};
}
Json utf16Position(TextEditor &editor,TextEditor::DocPos p){
    const auto text=editor.GetText();size_t at=0;for(size_t n=0;n<p.line;++n){at=text.find('\n',at);if(at==std::string::npos)throw cymcp::Error("InvalidParameters","Invalid selection line");++at;}
    size_t units=0;for(size_t glyph=0;glyph<p.index&&at<text.size()&&text[at]!='\n';++glyph){const auto c=static_cast<unsigned char>(text[at]);const size_t bytes=c>=0xf0?4:c>=0xe0?3:c>=0xc0?2:1;units+=bytes==4?2:1;at+=bytes;}
    return {{"line",p.line},{"character",units}};
}
}
void Reader::pollAutomation(){
    for(int count=0;count<16;++count){
        std::shared_ptr<AutomationRequest> request;
        {std::lock_guard lock(automationMutex_);if(automationRequests_.empty())break;request=automationRequests_.front();automationRequests_.pop_front();}
        try{request->context.cancellation.check();request->reply.set_value(automationDispatch(request->method,request->parameters,request->context));}
        catch(...){request->reply.set_exception(std::current_exception());}
    }
    if(assistant_)assistant_->poll();
    if(automation_){
        std::string key;for(const auto &[id,live]:core_.documents())key+=std::to_string(id)+":"+std::to_string(live->revision)+";";
        if(key!=automationRevisionKey_){automationRevisionKey_=std::move(key);automation_->changed();}
    }
}
void Reader::stopAutomationRequests(){
    std::lock_guard lock(automationMutex_);automationStopping_=true;
    for(auto &request:automationRequests_)try{throw cymcp::Error("Cancelled","Reader is shutting down; queued operation was not executed");}catch(...){request->reply.set_exception(std::current_exception());}
    automationRequests_.clear();
}
Json Reader::automationDispatch(const std::string &method,const Json &p,const cymcp::CallContext &context){
    context.cancellation.check();std::lock_guard lock(core_.mutex());context.cancellation.check();
    for(const auto &tab:tabs_){sync(tab);tab->live->readOnly=tab->readOnly();}core_.refresh();
    if(method=="ui.state"||method=="ui.preferences"){
        if(method=="ui.preferences"){
            if(p.contains("theme"))execute("view.theme",{{"theme",p.at("theme")}});
            if(p.contains("language"))execute("view.locale",{{"locale",p.at("language")}});
            for(const auto *key:{"wrap","minimap","whitespace"})if(p.contains(key)){
                const bool current=std::string(key)=="wrap"?wrap_:std::string(key)=="minimap"?minimap_:whitespace_;
                if(current!=p.at(key).get<bool>())execute("view.toggle",{{"option",key}});
            }
        }
        Json documents=Json::array();for(const auto &tab:tabs_)documents.push_back({{"id",tab->id},{"path",pathUtf8(tab->path())},{"revision",tab->live->revision},{"dirty",tab->dirty()},{"readOnly",tab->readOnly()},{"paged",bool(tab->paged)},{"canUndo",tab->canUndo()},{"canRedo",tab->canRedo()}});
        return {{"application","CyCodeReader"},{"active",active_},{"documents",documents},{"theme",theme_},{"language",locale_},{"wrap",wrap_},{"minimap",minimap_},{"whitespace",whitespace_},{"pendingClose",closeId_},{"pendingDialog",bool(dialog_)},{"pendingOverwrite",overwrite_.tab},{"quit",quit_}};
    }
    if(method=="ui.activate"){
        auto tab=find(p.at("id"));if(!tab)throw cymcp::Error("NotFound","Document is no longer open");active_=tab->id;tab->focus=true;return {{"active",active_}};
    }
    if(method.starts_with("editor.")){
        auto tab=find(p.at("id"));if(!tab)throw cymcp::Error("NotFound","Document is no longer open");
        if(tab->paged)throw cymcp::Error("UnsupportedOperation","This editor tool requires the ordinary profile; paged GUI ownership is not duplicated into another session");
        if(method=="editor.selection"){
            const auto selection=tab->editor.GetMainCursorSelection();
            return {{"id",tab->id},{"revision",tab->live->revision},{"positionEncoding","utf-16"},{"start",utf16Position(tab->editor,selection.start)},
                {"end",utf16Position(tab->editor,selection.end)},{"text",tab->editor.GetCursorText(0)}};
        }
        if(p.at("expectedRevision")!=tab->live->revision)throw cymcp::Error("RevisionConflict","Read the live document again");
        if(method=="editor.goto"){
            tab->editor.SetCursor(utf16Position(tab->editor,p.at("line"),p.at("character")));tab->focus=true;active_=tab->id;
            return {{"id",tab->id},{"revision",tab->live->revision},{"positionEncoding","utf-16"}};
        }
        if(tab->readOnly())throw cymcp::Error("ReadOnly","Document is read-only");
        if(method=="editor.replaceSelection")tab->editor.ReplaceTextPreservingTabs(p.at("text").get<std::string>());
        else if(method=="editor.undo")tab->editor.Undo();else if(method=="editor.redo")tab->editor.Redo();
        else throw cymcp::Error("UnknownOperation","Unknown editor operation");
        sync(tab);return core_.dispatch("documents.read",{{"id",tab->id}});
    }
    const auto result=dispatchTextSession(core_,method,p,context);
    if(method=="documents.close")erase(p.at("id"));
    for(const auto &[id,live]:core_.documents())if(!find(id)){auto tab=std::make_shared<Tab>(live);tab->id=id;install(tab);tabs_.push_back(tab);active_=id;}
    for(const auto &tab:tabs_)sync(tab);
    return result;
}
}
