#pragma once

#include "document.h"
#include "search_server.h"

#include <deque>

class RequestQueue {
public:
    explicit RequestQueue(const SearchServer& search_server);
    
    template <typename DocumentPredicate>
    std::vector<Document> AddFindRequest(const std::string& raw_query, DocumentPredicate document_predicate);
    std::vector<Document> AddFindRequest(const std::string& raw_query, DocumentStatus status);
    std::vector<Document> AddFindRequest(const std::string& raw_query);

    int GetNoResultRequests() const;
private:
    struct QueryResult {
        int time;
        bool empty_query;
    };
    std::deque<QueryResult> requests_;
    const static int sec_in_day_ = 1440;
    const SearchServer& search_server_;

    void AddQuery(const std::vector<Document>& documents);
};

template <typename DocumentPredicate>
std::vector<Document> RequestQueue::AddFindRequest(const std::string& raw_query, DocumentPredicate document_predicate) {
    auto doc = search_server_.FindTopDocuments(raw_query, document_predicate);
    AddQuery(doc);
    return doc;
}