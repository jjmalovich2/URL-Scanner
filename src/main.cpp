#include <iostream>
#include <string>
#include <fstream>
#include <filesystem>
#include <cstdint>
#include <regex>
#include <thread>
#include "heuristic.hpp"

// define npcap
#define HAVE_REMOTE
#include <pcap.h>

#include "gui.hpp"
#include "stats.hpp"
#include "dashboard.hpp"

URLHeuristic g_scanner;
std::filesystem::path g_exe_dir;
pcap_t* g_adhandle = nullptr;

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

    size_t host_pos = packet_content.find("Host: ");
    if (host_pos != std::string::npos) {
        size_t end_pos = packet_content.find("\r\n", host_pos);
        if (end_pos != std::string::npos) {
            found_url = packet_content.substr(host_pos + 6, end_pos - (host_pos + 6));
        }
    }

    if (found_url.empty()) {
        if (packet_content.find("http://") != std::string::npos || packet_content.find("https://") != std::string::npos) {
            size_t start = packet_content.find("http");
            size_t end = packet_content.find("\r\n");
            if (end != std::string::npos) {
                found_url = packet_content.substr(start, end - start);
            }
        }
    }

    if (found_url.empty()) {
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
        int score = g_scanner.calculate_score(found_url);
        double entropy = g_scanner.calculate_entropy(found_url);

        g_stats.total++;

        bool is_suspicious = (score >= 40 || entropy >= 3.8);
        if (is_suspicious) {
            g_stats.suspicious++;
        }

        bool is_action = (score >= 60 || entropy >= 4.8);
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
    g_exe_dir = std::filesystem::current_path();
    std::filesystem::path config_path = g_exe_dir / ".." / "config" / "keywords.csv";

    if (!g_scanner.load_keywords(config_path.string())) {
        std::cerr << "Failed to load keywords: " << config_path.string() << std::endl;
        return 1;
    }

    char errbuf[PCAP_ERRBUF_SIZE];
    pcap_if_t* alldevs;

    if (pcap_findalldevs_ex((char*)PCAP_SRC_IF_STRING, NULL, &alldevs, errbuf) == -1) {
        std::cerr << "Error finding network devices: " << errbuf << std::endl;
        return 1;
    }

    int i = 0;
    for (pcap_if_t* d_temp = alldevs; d_temp != nullptr; d_temp = d_temp->next) {
        std::cout << ++i << ". " << d_temp->name;
        if (d_temp->description) {
            std::cout << " (" << d_temp->description << ")";
        }
        std::cout << std::endl;
    }

    if (i == 0) {
        std::cerr << "No interfaces found! Make sure you are running as Administrator." << std::endl;
        return 1;
    }

    std::cout << "Enter the interface number to sniff (1-" << i << "): ";
    int interface_choice;
    std::cin >> interface_choice;

    if (interface_choice < 1 || interface_choice > i) {
        std::cerr << "Interface number out of range." << std::endl;
        pcap_freealldevs(alldevs);
        return 1;
    }

    pcap_if_t* d = alldevs;
    for (int j = 0; j < interface_choice - 1; j++) {
        d = d->next;
    }

    std::string friendly_name = (d->description != nullptr) ? d->description : d->name;
    std::cout << "\nSniffing system traffic on device: " << friendly_name << std::endl;

    g_adhandle = pcap_open(d->name, 65536, PCAP_OPENFLAG_PROMISCUOUS, 1000, NULL, errbuf);
    if (g_adhandle == NULL) {
        std::cerr << "Unable to open adapter: " << d->name << std::endl;
        pcap_freealldevs(alldevs);
        return 1;
    }

    struct bpf_program fcode;
    std::string filter_exp = "tcp port 80 or tcp port 443 or udp port 53";
    if (pcap_compile(g_adhandle, &fcode, filter_exp.c_str(), 1, 0xffffff) < 0) {
        std::cerr << "Filter compile error.\n";
        pcap_freealldevs(alldevs);
        return 1;
    }

    if (pcap_setfilter(g_adhandle, &fcode) < 0) {
        std::cerr << "Error setting network filter.\n";
        pcap_freealldevs(alldevs);
        return 1;
    }

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
