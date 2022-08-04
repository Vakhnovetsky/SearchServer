#include "process_queries.h"

#include <algorithm>
#include <execution>
#include <functional>
#include <utility>

using namespace std;

vector<vector<Document>> ProcessQueries(const SearchServer& search_server, const vector<string>& queries) {
    vector<vector<Document>> result(queries.size());
    transform(execution::par, queries.begin(), queries.end(), result.begin(),
              [&search_server](const string_view&query) { return search_server.FindTopDocuments(query); });
    return result;
}

list<Document> ProcessQueriesJoined(const SearchServer& search_server, const vector<string>& queries) {
    auto answers = ProcessQueries(search_server, queries);
    list<Document> result;
    for(auto& docs: answers) {
        for(auto& doc: docs) {
            result.push_back(move(doc));
        }
    }
    return result;
}