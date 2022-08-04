#include "string_processing.h"

#include <iostream>

using namespace std;

vector<string_view> SplitIntoWords(string_view text) {
    vector<string_view> words;
    const int64_t pos_end = text.npos;
    while (true) {
        int64_t space = text.find(' ', 0);
        words.push_back(space == pos_end ? text.substr(0) : text.substr(0, space));
        text.remove_prefix(space + 1);
        if (space == pos_end) {
            break;
        }
    }
    return words;
}