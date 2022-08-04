#include "request_queue.h"

using namespace std;

    RequestQueue::RequestQueue(const SearchServer& search_server): search_server_(search_server) {
    }

    vector<Document> RequestQueue::AddFindRequest(const string& raw_query, DocumentStatus status) {
        auto doc = search_server_.FindTopDocuments(raw_query, status);
        AddQuery(doc);
        return doc;
    }

    vector<Document> RequestQueue::AddFindRequest(const string& raw_query) {
        auto doc = search_server_.FindTopDocuments(raw_query);
        AddQuery(doc);
        return doc;
    }

    int RequestQueue::GetNoResultRequests() const {
        return count_if(requests_.begin(), requests_.end(), [](const RequestQueue::QueryResult& req) {return req.empty_query;} );
    }

    void RequestQueue::AddQuery(const vector<Document>& documents) {
        QueryResult query;
        if (requests_.empty()) {
            query.time = 1;
            }
            else {
                QueryResult backquery = requests_.back();
                if (backquery.time == sec_in_day_){
                    query.time = 1;
                } 
                else {
                    query.time = ++backquery.time;
                }
            }
            
        query.empty_query = documents.empty();
        requests_.push_back(query);
    
        while (requests_.size() > sec_in_day_){
            requests_.pop_front();
        }
    }          