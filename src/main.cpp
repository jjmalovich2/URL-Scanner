#include <iostream>
#include <string>
#include <fstream>
#include <filesystem>
#include <cstdint>
#include <regex>
#include <thread>
#include "heuristic.hpp"
#include "tls_parser.hpp"
#include "config.hpp"

// define npcap
#define HAVE_REMOTE
#include <pcap.h>

#include "gui.hpp"
#include "stats.hpp"
#include "dashboard.hpp"
#include "device_select.hpp"

URLHeuristic g_scanner;
std::filesystem::path g_exe_dir;
pcap_t* g_adhandle = nullptr;
int g_link_type = DLT_EN10MB;

void block_domain_in_hosts(const std::string& domain) {
    std::string hosts_path = "C:\\Windows\\System32\\drivers\\etc\\hosts";
    
    std::ifstream hosts_in(hosts_path);
    std::string line;
    while (std::getline(hosts_in, line)) {
        if (line.find(domain) != std::string::npos) {
            return; // Already blocked!
        }
    }
    hosts_in.close();

    std::ofstream hosts_out(hosts_path, std::ios::app);
    
    if (!hosts_out.is_open()) {
        std::cerr << "\n[ERROR] Failed to write to hosts file! Are you running as Administrator? Is Windows Defender blocking you?\n";
        return;
    }

    hosts_out << "\n0.0.0.0 " << domain << " # BLOCKED BY C++ SCANNER";
    std::cout << "\n[!] THREAT NEUTRALIZED: " << domain << " added to Windows hosts file.\n";
    hosts_out.close();
}

void packet_handler(u_char* param, const struct pcap_pkthdr* header, const u_char* pkt_data) {
    (void)param; (void)header;

    std::string packet_content(reinterpret_cast<const char*>(pkt_data), header->caplen);
    std::string found_url = "";

    // 1. Plaintext HTTP Host header
    size_t host_pos = packet_content.find("Host: ");
    if (host_pos != std::string::npos) {
        size_t end_pos = packet_content.find("\r\n", host_pos);
        if (end_pos != std::string::npos) {
            found_url = packet_content.substr(host_pos + 6, end_pos - (host_pos + 6));
        }
    }

    // 2. TLS Client Hello SNI (HTTPS traffic on port 443)
    if (found_url.empty()) {
        found_url = extract_sni(pkt_data, header->caplen, g_link_type);
    }

    // 3. Fallback HTTP URL patterns (skip for port 443 to avoid garbage from encrypted payloads)
    bool is_encrypted = is_tcp_dst_port_443(pkt_data, header->caplen, g_link_type);
    if (found_url.empty() && !is_encrypted) {
        if (packet_content.find("http://") != std::string::npos || packet_content.find("https://") != std::string::npos) {
            size_t start = packet_content.find("http");
            size_t end = packet_content.find("\r\n");
            if (end != std::string::npos) {
                found_url = packet_content.substr(start, end - start);
            }
        }
    }

    // 4. Regex domain fallback (skip for port 443 to avoid garbage)
    if (found_url.empty() && !is_encrypted) {
        try {
            std::regex domain_regex(R"([a-zA-Z0-9.-]+\.(com|net|org|io|co|us|gov|edu|tv|app))");
            std::smatch match;
            if (std::regex_search(packet_content.cbegin(), packet_content.cend(), match, domain_regex)) {
                found_url = match.str(0);
            }
        } catch (...) {
            // ignore regex errors
        }
    }

    if (!found_url.empty() && found_url.length() < 150) {
        g_stats.total++;

        if (g_scanner.is_whitelisted(found_url)) {
            std::cout << "[WHITELISTED] URL: " << found_url << std::endl;
            std::filesystem::path history_path = g_exe_dir / ".." / "scan_history.txt";
            std::ofstream history_log(history_path.string(), std::ios::app);
            if (history_log.is_open()) {
                history_log << "[WHITELISTED] URL: " << found_url << std::endl;
            }
            return;
        }

        if (g_scanner.is_blacklisted(found_url)) {
            std::cout << "\n[!] AUTO-BLOCKED blacklisted domain: " << found_url << std::endl;
            block_domain_in_hosts(found_url);
            g_stats.suspicious++;
            g_stats.blocked++;
            std::filesystem::path history_path = g_exe_dir / ".." / "scan_history.txt";
            std::ofstream history_log(history_path.string(), std::ios::app);
            if (history_log.is_open()) {
                history_log << "[AUTO-BLOCKED] URL: " << found_url << std::endl;
            }
            return;
        }

        int score = g_scanner.calculate_score(found_url);
        double entropy = g_scanner.calculate_entropy(found_url);

        bool is_suspicious = (score >= g_config.suspicious_score.load() || entropy >= g_config.suspicious_entropy.load());
        if (is_suspicious) {
            g_stats.suspicious++;
        }

        bool is_action = (score >= g_config.action_score.load() || entropy >= g_config.action_entropy.load());
        if (is_action) {
            std::cout << "\n[!] THRESHOLD EXCEEDED for " << found_url << " - showing alert dialog...\n";
            bool should_block = show_security_alert(found_url, score, entropy);
            if (should_block) {
                block_domain_in_hosts(found_url);
                g_stats.blocked++;
            } else {
                g_stats.allowed++;
                std::cout << "[ALLOWED] User chose to allow: " << found_url << std::endl;
            }
        }

        std::cout << "[CAPTURED] URL: " << found_url
                  << " | Score: " << score
                  << " | Entropy: " << entropy << std::endl;

        std::filesystem::path history_path = g_exe_dir / ".." / "scan_history.txt";
        std::ofstream history_log(history_path.string(), std::ios::app);
        if (history_log.is_open()) {
            history_log << "URL: " << found_url << " | Score: " << score << " | Entropy: " << entropy << std::endl;
        }
    }
}

void capture_loop() {
    pcap_loop(g_adhandle, -1, packet_handler, NULL);
}

int main() {
    SetProcessDPIAware();
    ShowWindow(GetConsoleWindow(), SW_HIDE);

    wchar_t exePathW[MAX_PATH];
    GetModuleFileNameW(nullptr, exePathW, MAX_PATH);
    g_exe_dir = std::filesystem::path(exePathW).parent_path();

    auto find_config = [&](const std::string& filename) -> std::filesystem::path {
        std::filesystem::path p1 = g_exe_dir / ".." / "config" / filename;
        if (std::filesystem::exists(p1)) return p1;
        std::filesystem::path p2 = g_exe_dir / "config" / filename;
        if (std::filesystem::exists(p2)) return p2;
        std::filesystem::path p3 = std::filesystem::current_path() / "config" / filename;
        if (std::filesystem::exists(p3)) return p3;
        return p1; // default so error messages point somewhere useful
    };

    std::filesystem::path config_path = find_config("keywords.csv");
    std::filesystem::path thresholds_path = find_config("thresholds.txt");

    if (!std::filesystem::exists(thresholds_path)) {
        std::filesystem::create_directories(thresholds_path.parent_path());
        save_thresholds(thresholds_path.string());
    }
    load_thresholds(thresholds_path.string());

    if (!std::filesystem::exists(config_path)) {
        std::filesystem::create_directories(config_path.parent_path());
        std::ofstream f(config_path.string());
        f << "phishing,40\nmalware,40\n";
    }

    if (!g_scanner.load_keywords(config_path.string())) {
        std::wstring msg = L"Failed to load keywords.\n\nTried:\n"
                         + urlscan::utf8_to_wstring((g_exe_dir / ".." / "config" / "keywords.csv").string())
                         + L"\n" + urlscan::utf8_to_wstring((g_exe_dir / "config" / "keywords.csv").string())
                         + L"\n\nMake sure the config folder exists next to the scanner executable.";
        MessageBoxW(nullptr, msg.c_str(), L"URL Scanner Error", MB_OK | MB_ICONERROR);
        return 1;
    }

    std::filesystem::path whitelist_path = find_config("whitelist.txt");
    if (!std::filesystem::exists(whitelist_path)) {
        std::filesystem::create_directories(whitelist_path.parent_path());
        std::ofstream f(whitelist_path.string());
        f << "# Domain whitelist\n";
    }
    g_scanner.load_whitelist(whitelist_path.string());

    std::filesystem::path blacklist_path = find_config("blacklist.txt");
    if (!std::filesystem::exists(blacklist_path)) {
        std::filesystem::create_directories(blacklist_path.parent_path());
        std::ofstream f(blacklist_path.string());
        f << "# Domain blacklist\n";
    }
    g_scanner.load_blacklist(blacklist_path.string());

    char errbuf[PCAP_ERRBUF_SIZE];
    pcap_if_t* alldevs;

    if (pcap_findalldevs_ex((char*)PCAP_SRC_IF_STRING, NULL, &alldevs, errbuf) == -1) {
        std::wstring msg = L"Error finding network devices:\n" + urlscan::utf8_to_wstring(errbuf)
                         + L"\n\nMake sure Npcap is installed and you are running as Administrator.";
        MessageBoxW(nullptr, msg.c_str(), L"URL Scanner Error", MB_OK | MB_ICONERROR);
        return 1;
    }

    int device_count = 0;
    for (pcap_if_t* d_temp = alldevs; d_temp != nullptr; d_temp = d_temp->next) {
        device_count++;
    }

    if (device_count == 0) {
        MessageBoxW(nullptr, L"No network interfaces found.\n\nMake sure you are running as Administrator.",
            L"URL Scanner Error", MB_OK | MB_ICONERROR);
        pcap_freealldevs(alldevs);
        return 1;
    }

    int selected = show_device_selector(alldevs);
    if (selected < 0 || selected >= device_count) {
        pcap_freealldevs(alldevs);
        return 1;
    }

    pcap_if_t* d = alldevs;
    for (int j = 0; j < selected; j++) {
        d = d->next;
    }

    std::string friendly_name = (d->description != nullptr) ? d->description : d->name;
    std::cout << "\nSelected device: " << friendly_name << std::endl;
    std::cout << "Internal name:   " << d->name << std::endl;

    // Warn about common virtual/disconnected adapters
    std::string lower_fn = friendly_name;
    for (char& c : lower_fn) c = static_cast<char>(tolower(static_cast<unsigned char>(c)));
    if (lower_fn.find("bluetooth") != std::string::npos ||
        lower_fn.find("wan miniport") != std::string::npos ||
        lower_fn.find("loopback") != std::string::npos) {
        std::cout << "\n[WARNING] This is a virtual/secondary adapter. If you see no traffic,\n";
        std::cout << "          close the scanner and pick your Wi-Fi or Ethernet adapter instead.\n";
    }

    g_adhandle = pcap_open(d->name, 65536, PCAP_OPENFLAG_PROMISCUOUS, 1000, NULL, errbuf);
    if (g_adhandle == NULL) {
        std::wstring msg = L"Unable to open adapter:\n" + urlscan::utf8_to_wstring(d->name)
                         + L"\n\nMake sure you are running as Administrator.";
        MessageBoxW(nullptr, msg.c_str(), L"URL Scanner Error", MB_OK | MB_ICONERROR);
        pcap_freealldevs(alldevs);
        return 1;
    }

    g_link_type = pcap_datalink(g_adhandle);
    std::cout << "Link type: " << pcap_datalink_val_to_name(g_link_type)
              << " (" << pcap_datalink_val_to_description(g_link_type) << ")" << std::endl;

    struct bpf_program fcode;
    std::string filter_exp = "tcp port 80 or tcp port 443 or udp port 53";
    if (pcap_compile(g_adhandle, &fcode, filter_exp.c_str(), 1, 0xffffff) < 0) {
        MessageBoxW(nullptr, L"Filter compile error.", L"URL Scanner Error", MB_OK | MB_ICONERROR);
        pcap_close(g_adhandle);
        pcap_freealldevs(alldevs);
        return 1;
    }

    if (pcap_setfilter(g_adhandle, &fcode) < 0) {
        MessageBoxW(nullptr, L"Error setting network filter.", L"URL Scanner Error", MB_OK | MB_ICONERROR);
        pcap_freecode(&fcode);
        pcap_close(g_adhandle);
        pcap_freealldevs(alldevs);
        return 1;
    }

    pcap_freecode(&fcode);

    std::cout << "\n>>> Scanner Running <<<\n\n";
    pcap_freealldevs(alldevs);

    std::thread capture_thread(capture_loop);

    // Main thread runs the dashboard GUI
    run_dashboard();

    // Dashboard closed - stop capture
    pcap_breakloop(g_adhandle);
    capture_thread.join();

    pcap_close(g_adhandle);
    return 0;
}
