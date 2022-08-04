#pragma once

#include <set>
#include <string_view>
#include <vector>

std::vector<std::string_view> SplitIntoWords(std::string_view text);

template <typename StringContainer>
std::set<std::string_view> MakeUniqueNonEmptyStrings(StringContainer strings) {
    std::set<std::string_view> non_empty_strings;
    for (std::string_view str : strings) {
        if (!str.substr().empty()) {
            non_empty_strings.insert(str);
        }
    }
    return non_empty_strings;
}