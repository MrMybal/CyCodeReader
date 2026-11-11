#pragma once
#include <string>
#include <string_view>

namespace cycode::detail {
// CommandLineToArgvW treats backslashes before quotes specially. In particular,
// a quoted directory's trailing backslashes must be doubled before the last quote.
// https://learn.microsoft.com/windows/win32/api/shellapi/nf-shellapi-commandlinetoargvw
constexpr std::wstring quoteWindowsArgument(std::wstring_view value) {
    std::wstring result=L"\"";
    std::size_t slashes=0;
    for (const auto c : value) {
        if (c==L'\\') { ++slashes;continue; }
        result.append(c==L'"' ? slashes*2+1 : slashes,L'\\');
        slashes=0;
        result+=c;
    }
    result.append(slashes*2,L'\\');
    result+=L'"';
    return result;
}
} // namespace cycode::detail
