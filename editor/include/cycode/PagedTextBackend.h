#pragma once
#include <cyui/modules/PagedCodeEditor.h>
#include <filesystem>
#include <functional>

namespace cycode {
struct PagedTextMatch {
    std::uint64_t offset=0,length=0; // UTF-8 byte offsets in the native document
    cyui::PagedCodeSelection range; // UTF-16, never a byte cursor
    std::string preview;
};
struct PagedTextStatus {
    bool ready=false,busy=true,dirty=false,readOnly=false,canUndo=false,canRedo=false;
    std::uint64_t revision=0,bytes=0,lines=1,searchGeneration=0,searchedRevision=0;
    bool searching=false,truncated=false;
    std::filesystem::path path;
    std::string newline="\n",message,syntaxMessage;
    std::vector<PagedTextMatch> matches;
};
struct PagedTextBytes {
    std::string text; // valid complete UTF-8 scalars, physical newlines/BOM unchanged
    std::uint64_t nextOffset=0; // byte continuation; MUST equal requested offset + text.size()
};
struct PagedTextPreparedEdit {
    std::string token; // opaque native preview/transaction identity
    cyui::ItemId document=0;
    std::uint64_t revision=0;
};
struct PagedTextCommit {
    PagedTextStatus status;
    cyui::PagedCodeSelection selection;
};
struct PagedTextSearch {
    std::vector<PagedTextMatch> matches;
    bool truncated=false;
};
// Injected presentation backend. Implementations route to the host's EXISTING
// native owner (e.g. IDE Engine); the adapter creates no LargeText/session/Undo
// in this mode. Methods run on the adapter worker, never inside draw/UI.
// The host must retain its owner/transport until adapter destruction has joined.
// Every operation must validate identity/revision at the actual native boundary.
class PagedTextBackend {
  public:
    using Cancelled=std::function<bool()>;
    virtual ~PagedTextBackend()=default;
    virtual PagedTextStatus state(cyui::ItemId document)=0;
    // Resolve once, then continue reads by byte nextOffset. Repeated UTF-16
    // readLines continuations can rescan a huge line's prefix quadratically.
    virtual std::uint64_t offset(cyui::ItemId,std::uint64_t revision,cyui::PagedCodePosition)=0;
    virtual PagedTextBytes readBytes(cyui::ItemId,std::uint64_t revision,
                                    std::uint64_t byteOffset,std::size_t maxBytes)=0;
    // Complete lexical context belongs to the native/common lexer. Returned spans
    // are fragment-local UTF-8, sorted and non-overlapping. Empty with an explicit
    // state().syntaxMessage when contextual syntax is unavailable; no page reset.
    virtual std::vector<cyui::CodeToken> syntax(cyui::ItemId,std::uint64_t revision,cyui::CodeLanguage,
                                             std::uint64_t byteOffset,std::size_t bytes)=0;
    // Prepare has no visible content effect; apply commits ONE native Undo
    // transaction and returns its authoritative revision/selection. A thrown apply
    // must mean no commit; post-commit warnings belong in the returned status.
    // Engine backends map the matching commit.documents receipt directly; do
    // not reread state after commit to acknowledge an edit (another edit may win).
    // notificationsComplete=false is success with a warning, never rejection.
    // An RPC transport loss is unresolved: reconcile the issued plan through
    // changes.status before returning/throwing. Unknown/evicted status forbids
    // replay; keep the pending mutation unresolved until its outcome is known.
    // Replacement.text is raw UTF-8 input; native owner applies its newline policy.
    virtual PagedTextPreparedEdit prepare(const cyui::PagedCodeReplacement &)=0;
    virtual PagedTextCommit apply(const PagedTextPreparedEdit &)=0;
    virtual void discard(const PagedTextPreparedEdit &) noexcept=0; // idempotent preview cleanup
    // Native historyStatus/historyPrepare guards cover the history revision
    // and ALL affected documents. Apply the new preview and return its receipt.
    // Snapshot replay may legitimately contain no textual edits.
    virtual PagedTextCommit undo(cyui::ItemId,std::uint64_t revision,bool redo)=0;
    virtual PagedTextStatus setReadOnly(cyui::ItemId,std::uint64_t revision,bool)=0;
    virtual std::string copy(cyui::ItemId,std::uint64_t revision,
                             cyui::PagedCodeSelection,std::size_t maxBytes)=0;
    virtual PagedTextSearch search(cyui::ItemId,std::uint64_t revision,const std::string &,
                                   bool sensitive,bool words,std::size_t maxResults,
                                   const Cancelled &)=0;
    // Native owner publishes the ENTIRE document; empty destination means Save.
    // Save As preserves the original and refuses existing destinations/conflicts.
    virtual PagedTextStatus save(cyui::ItemId,std::uint64_t revision,
                                 const std::filesystem::path &destination)=0;
    // Durable ownership stays native. Unsupported backends explicitly refuse.
    virtual PagedTextStatus checkpoint(cyui::ItemId,std::uint64_t revision)=0;
};
}
