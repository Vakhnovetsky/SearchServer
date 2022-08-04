#pragma once

#include "concurrent_map.h"
#include "document.h"
#include "log_duration.h"
#include "read_input_functions.h"
#include "string_processing.h"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <execution>
#include <future>
#include <iostream>
#include <map>
#include <mutex>
#include <numeric>
#include <set>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

class SearchServer {
public:
    template <typename StringContainer>
    explicit SearchServer(const StringContainer& stop_words);
    explicit SearchServer(const std::string& stop_words_view);
    explicit SearchServer(std::string_view stop_words_view);

    void AddDocument(int document_id, std::string_view document, DocumentStatus status, const std::vector<int>& ratings);

    template <typename DocumentPredicate>
    std::vector<Document> FindTopDocuments(std::string_view raw_query, DocumentPredicate document_predicate) const;
    std::vector<Document> FindTopDocuments(std::string_view raw_query, DocumentStatus status) const;
    std::vector<Document> FindTopDocuments(std::string_view raw_query) const;

    template<typename ExecutionPolicy, typename DocumentPredicate>
    std::vector<Document> FindTopDocuments(const ExecutionPolicy& policy, std::string_view raw_query, DocumentPredicate document_predicate) const;
    template<typename ExecutionPolicy>
    std::vector<Document> FindTopDocuments(const ExecutionPolicy& policy, std::string_view raw_query, DocumentStatus status) const;
    template<typename ExecutionPolicy>
    std::vector<Document> FindTopDocuments(const ExecutionPolicy& policy, std::string_view raw_query) const;

    int GetDocumentCount() const;

    template<typename ExecutionPolicy>
    std::tuple<std::vector<std::string_view>, DocumentStatus> MatchDocument(const ExecutionPolicy& policy, std::string_view raw_query, int document_id) const;
    std::tuple<std::vector<std::string_view>, DocumentStatus> MatchDocument(std::string_view raw_query, int document_id) const;

    std::set<int>::const_iterator begin() const;
    std::set<int>::const_iterator end() const;

    const std::map<std::string_view, double>& GetWordFrequencies(int document_id) const;

    template<typename ExecutionPolicy>
    void RemoveDocument(const ExecutionPolicy& policy, int document_id);
    void RemoveDocument(int document_id);

private:
    struct DocumentData {
        int rating;
        DocumentStatus status;
        std::set<std::string, std::less<>> words;
    };
    std::set<std::string, std::less<>> stop_words_;
    std::map<std::string_view, std::map<int, double>> word_to_document_freqs_;
    std::map<int, std::map<std::string_view, double>> document_to_word_freqs_;
    std::map<int, DocumentData> documents_;
    std::set<int> document_ids_;

    bool IsStopWord(std::string_view word) const;

    static bool IsValidWord(std::string_view word);

    std::vector<std::string_view> SplitIntoWordsNoStop(std::string_view text) const;

    static int ComputeAverageRating(const std::vector<int>& ratings);

    struct QueryWord {
        std::string_view data;
        bool is_minus;
        bool is_stop;
    };

    QueryWord ParseQueryWord(std::string_view text) const;

    struct Query {
        std::set<std::string_view> plus_words;
        std::set<std::string_view> minus_words;
    };

    Query ParseQuery(std::string_view text) const;

    double ComputeWordInverseDocumentFreq(std::string_view word) const;

    template <typename DocumentPredicate>
    std::vector<Document> FindAllDocuments(const std::execution::parallel_policy&, const Query& query, DocumentPredicate document_predicate) const;
    template <typename DocumentPredicate>
    std::vector<Document> FindAllDocuments(const std::execution::sequenced_policy&, const Query& query, DocumentPredicate document_predicate) const;
};

template <typename StringContainer>
SearchServer::SearchServer(const StringContainer& stop_words) {
    std::set<std::string_view> words = MakeUniqueNonEmptyStrings(stop_words);
    for (const std::string_view& word : words) {
        stop_words_.emplace(word.substr());
    }

    using namespace std::string_literals;
    if (!all_of(stop_words_.begin(), stop_words_.end(), IsValidWord)) {
        throw std::invalid_argument("Some of stop words are invalid"s);
    }
}

template<typename ExecutionPolicy, typename DocumentPredicate>
std::vector<Document> SearchServer::FindTopDocuments(const ExecutionPolicy& policy, std::string_view raw_query, DocumentPredicate document_predicate) const {
    
    const Query query = ParseQuery(raw_query);
    
    std::vector<Document> matched_documents = FindAllDocuments(policy, query, document_predicate);
    
    std::sort(policy, matched_documents.begin(), matched_documents.end(), [](const Document& lhs, const Document& rhs) {
        if (std::abs(lhs.relevance - rhs.relevance) < 1e-6) {
            return lhs.rating > rhs.rating;
        }
        else {
            return lhs.relevance > rhs.relevance;
        }
        });
    
    if (matched_documents.size() > MAX_RESULT_DOCUMENT_COUNT) {
        matched_documents.resize(MAX_RESULT_DOCUMENT_COUNT);
    }
    
    return matched_documents;
}

template <typename DocumentPredicate>
std::vector<Document> SearchServer::FindTopDocuments(std::string_view raw_query, DocumentPredicate document_predicate) const {
    return FindTopDocuments(std::execution::seq, raw_query, document_predicate);
}

template<typename ExecutionPolicy>
std::vector<Document> SearchServer::FindTopDocuments(const ExecutionPolicy& policy, std::string_view raw_query, DocumentStatus status) const {
    return FindTopDocuments(policy, raw_query,
        [status](int document_id, DocumentStatus document_status, int rating) {
            return document_status == status;
        });
}

template<typename ExecutionPolicy>
std::vector<Document> SearchServer::FindTopDocuments(const ExecutionPolicy& policy, std::string_view raw_query) const {
    return FindTopDocuments(policy, raw_query, DocumentStatus::ACTUAL);
}

template <typename DocumentPredicate>
std::vector<Document> SearchServer::FindAllDocuments(const std::execution::sequenced_policy&, const Query& query, DocumentPredicate document_predicate) const {
    ConcurrentMap<int, double> document_to_relevance(query.plus_words.size());
    ConcurrentSet<int> bad_documents(document_ids_.size());

    for (std::string_view word : query.minus_words) {
        if (word_to_document_freqs_.count(word) != 0) {
            for (const auto& [document_id, term_freq] : word_to_document_freqs_.at(word)) {
                bad_documents.Insert(document_id);
            }
        }
    };

    for (std::string_view word : query.plus_words) {
        if (word_to_document_freqs_.count(word) == 0) {
            continue;
         }
        const double inverse_document_freq = ComputeWordInverseDocumentFreq(word);
        for (const auto & [document_id, term_freq] : word_to_document_freqs_.at(word)) {
            const auto &document_data = documents_.at(document_id);
            if (!bad_documents.Contains(document_id) && document_predicate(document_id, document_data.status, document_data.rating)) {   
                document_to_relevance[document_id].ref_to_value += term_freq * inverse_document_freq;
            }
        }
    }

    std::vector<Document> matched_documents;
    for (const auto [document_id, relevance] : document_to_relevance.BuildOrdinaryMap()) {
        matched_documents.push_back({ document_id, relevance, documents_.at(document_id).rating });
    }
    return matched_documents;
}

template <typename DocumentPredicate>
std::vector<Document> SearchServer::FindAllDocuments(const std::execution::parallel_policy&, const Query& query, DocumentPredicate document_predicate) const {
    ConcurrentMap<int, double> document_to_relevance(query.plus_words.size());
    ConcurrentSet<int> bad_documents(document_ids_.size());

    std::for_each(std::execution::par, query.minus_words.begin(), query.minus_words.end(),
        [this, &bad_documents](std::string_view word) {
            if (word_to_document_freqs_.count(word) != 0) {
                for (const auto& [document_id, term_freq] : word_to_document_freqs_.at(word)) {
                    bad_documents.Insert(document_id);
                }
            }
        });

    static constexpr int PART_COUNT = 10;
    const auto part_length = query.plus_words.size() / PART_COUNT;
    auto part_begin = query.plus_words.begin();
    auto part_end = std::next(part_begin, part_length);

    auto function = [&](const std::string_view& word) {
        const double inverse_document_freq = ComputeWordInverseDocumentFreq(word);
        for (const auto& [document_id, term_freq] : word_to_document_freqs_.at(word)) {
            if (!bad_documents.Contains(document_id)) {
                document_to_relevance[document_id].ref_to_value += term_freq * inverse_document_freq;
            }
        }
    };

    std::vector<std::future<void>> futures;
    for (int i = 0;	i < PART_COUNT;	++i,
        part_begin = part_end, part_end = (i == PART_COUNT - 1 ? query.plus_words.end() : next(part_begin, part_length))) {
        futures.push_back(std::async([function, part_begin, part_end] {
            std::for_each(part_begin, part_end, function);
            }));
    }
    for (int i = 0; i < futures.size(); ++i) {
        futures[i].get();
    }

    std::vector<Document> matched_documents;
    for (const auto [document_id, relevance] : document_to_relevance.BuildOrdinaryMap()) {
        matched_documents.push_back({ document_id, relevance, documents_.at(document_id).rating });
    }
    return matched_documents;
}

template<typename ExecutionPolicy>
void SearchServer::RemoveDocument(const ExecutionPolicy& policy, int document_id) {
    if (document_ids_.count(document_id) > 0) {
        document_ids_.erase(document_id);
        documents_.erase(document_id);
        document_to_word_freqs_.erase(document_id);

        std::for_each(policy, word_to_document_freqs_.begin(), word_to_document_freqs_.end(),
            [document_id](auto& word_freqs) {
                if (word_freqs.second.count(document_id) > 0) {
                    word_freqs.second.erase(document_id);
                }
            });
    }
}

template<typename ExecutionPolicy>
std::tuple<std::vector<std::string_view>, DocumentStatus> SearchServer::MatchDocument(const ExecutionPolicy& policy, std::string_view raw_query, int document_id) const {
    const auto query = ParseQuery(raw_query);

    if (std::any_of(policy, query.minus_words.begin(), query.minus_words.end(), [document_id, this](std::string_view word) {
        return this->word_to_document_freqs_.count(word) != 0 && this->word_to_document_freqs_.at(word).count(document_id) > 0; })) {
        return { {}, documents_.at(document_id).status };
    }

    std::vector<std::string_view> matched_words;

    std::copy_if(query.plus_words.begin(), query.plus_words.end(), std::back_inserter(matched_words),
        [document_id, this](std::string_view word) {
            return (this->word_to_document_freqs_.count(word) != 0 && this->word_to_document_freqs_.at(word).count(document_id) > 0);
        });

    std::sort(policy, matched_words.begin(), matched_words.end());
    std::unique(policy, matched_words.begin(), matched_words.end());

    return { matched_words, documents_.at(document_id).status };
}