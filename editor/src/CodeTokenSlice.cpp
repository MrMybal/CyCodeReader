#include "cycode/CodeTokenSlice.h"
#include <algorithm>
#include <limits>
#include <stdexcept>
namespace cycode {
namespace {
bool boundary(std::string_view text,std::size_t at) {
    return at==text.size() || (static_cast<unsigned char>(text[at])&0xc0)!=0x80;
}
std::size_t endOf(const cyui::CodeToken &token) {
    if(token.length>std::numeric_limits<std::size_t>::max()-token.start)
        throw std::runtime_error("Syntax token byte offset overflow");
    return token.start+token.length;
}
}
void validateCodeTokens(std::string_view fragment,const std::vector<cyui::CodeToken> &tokens) {
    std::size_t previous=0;
    for(const auto &token:tokens) {
        const auto end=endOf(token);
        if(!token.length || token.start<previous || end>fragment.size() ||
           !boundary(fragment,token.start) || !boundary(fragment,end) ||
           token.role<cyui::TokenRole::text || token.role>=cyui::TokenRole::count)
            throw std::runtime_error("Invalid fragment-local UTF-8 syntax span");
        previous=end;
    }
}
std::vector<cyui::CodeToken> clipCodeTokens(const std::vector<cyui::CodeToken> &tokens,
                                         std::uint64_t start,std::string_view fragment) {
    if(fragment.size()>UINT64_MAX-start)throw std::runtime_error("Syntax fragment byte offset overflow");
    const auto end=start+fragment.size();std::vector<cyui::CodeToken> result;
    auto token=std::lower_bound(tokens.begin(),tokens.end(),start,[](const auto &t,auto value){return endOf(t)<=value;});
    for(;token!=tokens.end() && token->start<end;++token) {
        const auto a=std::max<std::uint64_t>(start,token->start),b=std::min<std::uint64_t>(end,endOf(*token));
        if(a<b)result.push_back({static_cast<std::size_t>(a-start),static_cast<std::size_t>(b-a),token->role});
    }
    validateCodeTokens(fragment,result);return result;
}
}
