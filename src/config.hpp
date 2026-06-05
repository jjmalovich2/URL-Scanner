#pragma once
#ifndef CONFIG_HPP
#define CONFIG_HPP

#include <atomic>
#include <string>
#include <fstream>
#include <sstream>
#include <iostream>
#include <filesystem>

struct AppConfig {
    std::atomic<int> suspicious_score{40};
    std::atomic<double> suspicious_entropy{3.0};
    std::atomic<int> action_score{60};
    std::atomic<double> action_entropy{4.8};
};

inline AppConfig g_config;

extern std::filesystem::path g_exe_dir;

inline void load_thresholds(const std::string& path) {
    std::ifstream f(path);
    if (!f.is_open()) return;

    std::string line;
    while (std::getline(f, line)) {
        if (line.empty() || line[0] == '#') continue;
        size_t eq = line.find('=');
        if (eq == std::string::npos) continue;
        std::string key = line.substr(0, eq);
        std::string val = line.substr(eq + 1);

        try {
            if (key == "suspicious_score") g_config.suspicious_score = std::stoi(val);
            else if (key == "suspicious_entropy") g_config.suspicious_entropy = std::stod(val);
            else if (key == "action_score") g_config.action_score = std::stoi(val);
            else if (key == "action_entropy") g_config.action_entropy = std::stod(val);
        } catch (...) {
            std::cerr << "Invalid threshold value: " << key << "=" << val << std::endl;
        }
    }
}

inline void save_thresholds(const std::string& path) {
    std::ofstream f(path);
    if (!f.is_open()) {
        std::cerr << "Failed to save thresholds to " << path << std::endl;
        return;
    }
    f << "suspicious_score=" << g_config.suspicious_score.load() << "\n";
    f << "suspicious_entropy=" << g_config.suspicious_entropy.load() << "\n";
    f << "action_score=" << g_config.action_score.load() << "\n";
    f << "action_entropy=" << g_config.action_entropy.load() << "\n";
}

inline std::string read_file_text(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    if (!f) return "";
    return std::string((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
}

inline bool write_file_text(const std::string& path, const std::string& content) {
    std::ofstream f(path, std::ios::binary);
    if (!f) return false;
    f << content;
    return f.good();
}

#endif // CONFIG_HPP
