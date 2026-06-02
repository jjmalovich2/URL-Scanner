#pragma once
#ifndef HEURISTIC_HPP
#define HEURISTIC_HPP

#include <string>
#include <string_view>
#include <vector>
#include <set>
#include <unordered_map>
#include <sstream>
#include <iostream>
#include <cmath>
#include <fstream>
#include <regex>
#include <algorithm>

class URLHeuristic {
private:
    std::unordered_map<std::string, int> suspicious_keywords;

public:
    // load csv
    bool load_keywords(const std::string& file_path) {
        std::ifstream file(file_path);
        if (!file.is_open()) {
            std::cerr << "Failed to open file: " << file_path << std::endl;
            return false;
        }

        std::string line;
        while (std::getline(file, line)) {
            if (line.empty()) continue;

            std::stringstream ss(line);
            std::string keyword;
            std::string score_str;

            if (std::getline(ss, keyword, ',') && std::getline(ss, score_str)) {
                try {
                    int score = std::stoi(score_str);
                    suspicious_keywords[keyword] = std::stoi(score_str);
                } catch (const std::exception& e) {
                    std::cerr << "Invalid score for keyword: " << keyword << " in file: " << file_path << std::endl;
                }
            }
        }

        return true;
    }

    // calculate shannon entropy
    double calculate_entropy(std::string_view str) const {
        if (str.empty()) return 0.0;

        std::unordered_map<char, int> frequencies;
        for (char c : str) frequencies[c]++;

        double entropy = 0.0;
        double length = static_cast<double>(str.length());

        for (const auto& pair : frequencies) {
            double probability = pair.second / length;
            entropy -= probability * std::log2(probability);
        }

        return entropy;
    }

    int calculate_score(const std::string& url) const {
        int score = 0;
        std::string lower_url = url;
        std::transform(lower_url.begin(), lower_url.end(), lower_url.begin(), ::tolower);

        for (const auto& [keyword, weight] : suspicious_keywords) {
            std::string pattern = "\\b" + keyword + "\\b";
            std::regex word_regex(pattern);
            std::smatch match;

            if (std::regex_search(lower_url.cbegin(), lower_url.cend(), match, word_regex)) {
                score += weight;
            }
        }

        if (url.find("http://") == 0 && url.length() > 7 && std::isdigit(url[7])) {
            score += 40;
        }

        return score;
    }
};

#endif // HEURISTIC_HPP