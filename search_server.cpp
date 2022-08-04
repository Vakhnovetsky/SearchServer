#include "search_server.h"
#include "string_processing.h"

using namespace std;

SearchServer::SearchServer(const string& stop_words_view)
    : SearchServer(SplitIntoWords(stop_words_view)) {
}

SearchServer::SearchServer(string_view stop_words_view)
    : SearchServer(SplitIntoWords(stop_words_view)) {
}

void SearchServer::AddDocument(int document_id, string_view document, DocumentStatus status, const vector<int>& ratings) {
    if ((document_id < 0) || (documents_.count(document_id) > 0)) {
        throw invalid_argument("Invalid document_id"s);
    }
    const auto words = SplitIntoWordsNoStop(document);

    const double inv_word_count = 1.0 / words.size();
    set<string, less<>> doc_words;
    for (string_view word : words) {
        doc_words.emplace(word.substr());
        auto iter = doc_words.find(word);
        word_to_document_freqs_[*iter][document_id] += inv_word_count;
        document_to_word_freqs_[document_id][*iter] += inv_word_count;
    }
    documents_.emplace(document_id, DocumentData{ ComputeAverageRating(ratings), status, move(doc_words) });
    document_ids_.insert(document_id);
}

vector<Document> SearchServer::FindTopDocuments(string_view raw_query, DocumentStatus status) const {
    return FindTopDocuments(raw_query, [status](int document_id, DocumentStatus document_status, int rating) {
        return document_status == status;
        });
}

vector<Document> SearchServer::FindTopDocuments(string_view raw_query) const {
    return FindTopDocuments(raw_query, DocumentStatus::ACTUAL);
}

int SearchServer::GetDocumentCount() const {
    return documents_.size();
}

std::set<int>::const_iterator SearchServer::begin() const {
    return document_ids_.begin();
}

std::set<int>::const_iterator SearchServer::end() const {
    return document_ids_.end();
}

const map<string_view, double>& SearchServer::GetWordFrequencies(int document_id) const {
    static map<string_view, double> empty_map;
    if (document_to_word_freqs_.count(document_id) > 0) {
        return document_to_word_freqs_.at(document_id);
    }
    return empty_map;
}

tuple<vector<string_view>, DocumentStatus> SearchServer::MatchDocument(string_view raw_query, int document_id) const {
    return MatchDocument(std::execution::seq, raw_query, document_id);
}

void SearchServer::RemoveDocument(int document_id) {
    RemoveDocument(std::execution::seq, document_id);
}

//private:
bool SearchServer::IsStopWord(string_view word) const {
    return stop_words_.count(word) > 0;
}

bool SearchServer::IsValidWord(string_view word) {
    // A valid word must not contain special characters
    return none_of(word.begin(), word.end(), [](char c) {
        return c >= '\0' && c < ' ';
        });
}

vector<string_view> SearchServer::SplitIntoWordsNoStop(string_view text) const {
    vector<string_view> words;
    for (const string_view& word : SplitIntoWords(text)) {
        if (!IsValidWord(word)) {
            string s(word.substr());
            throw invalid_argument("Word "s + s + " is invalid");
        }
        if (!IsStopWord(word)) {
            words.push_back(word);
        }
    }
    return words;
}

SearchServer::Query SearchServer::ParseQuery(string_view text) const {
    SearchServer::Query result;
    for (const string_view& word : SplitIntoWords(text)) {
        const auto query_word = ParseQueryWord(word);
        if (!query_word.is_stop) {
            if (query_word.is_minus) {
                result.minus_words.insert(query_word.data.substr());
            }
            else {
                result.plus_words.insert(query_word.data.substr());
            }
        }
    }

    return result;
}

int SearchServer::ComputeAverageRating(const vector<int>& ratings) {
    if (ratings.empty()) {
        return 0;
    }
    int rating_sum = 0;
    for (const int rating : ratings) {
        rating_sum += rating;
    }
    return rating_sum / static_cast<int>(ratings.size());
}

SearchServer::QueryWord SearchServer::ParseQueryWord(string_view text) const {
    if (text.empty()) {
        throw invalid_argument("Query word is empty"s);
    }
    string word(text.substr());
    bool is_minus = false;
    string_view new_text = text;
    if (word[0] == '-') {
        is_minus = true;
        word = word.substr(1);
        new_text = text.substr(1);
    }
    if (word.empty() || word[0] == '-' || !IsValidWord(word)) {
        throw invalid_argument("Query word "s + word + " is invalid");
    }
    return { new_text, is_minus, IsStopWord(word) };
}

double SearchServer::ComputeWordInverseDocumentFreq(string_view word) const {
    return log(GetDocumentCount() * 1.0 / word_to_document_freqs_.at(word).size());
}