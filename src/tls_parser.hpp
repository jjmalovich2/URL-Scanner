#pragma once
#ifndef TLS_PARSER_HPP
#define TLS_PARSER_HPP

#include <string>
#include <cstdint>
#include <pcap.h>

namespace {
    inline uint16_t read_u16_be(const uint8_t* p) {
        return (static_cast<uint16_t>(p[0]) << 8) | static_cast<uint16_t>(p[1]);
    }

    inline uint32_t read_u24_be(const uint8_t* p) {
        return (static_cast<uint32_t>(p[0]) << 16)
             | (static_cast<uint32_t>(p[1]) << 8)
             | static_cast<uint32_t>(p[2]);
    }

    inline std::string trim_nulls(const std::string& s) {
        size_t end = s.find('\0');
        if (end == std::string::npos) return s;
        return s.substr(0, end);
    }
}

// Extract the Server Name Indication (SNI) hostname from a raw packet.
// link_type should come from pcap_datalink(). Supports DLT_EN10MB (Ethernet)
// and DLT_NULL (loopback). Returns empty string if no SNI is found.
// Note: does not perform TCP reassembly.
inline std::string extract_sni(const uint8_t* pkt, size_t pkt_len, int link_type) {
    size_t l2_hdr_len = 0;
    uint16_t ethertype = 0;

    if (link_type == DLT_EN10MB) {
        // Ethernet II (14 bytes) or VLAN tagged (18 bytes)
        if (pkt_len < 14) return "";
        ethertype = read_u16_be(pkt + 12);
        l2_hdr_len = 14;
        if (ethertype == 0x8100) {
            // 802.1Q VLAN
            if (pkt_len < 18) return "";
            ethertype = read_u16_be(pkt + 16);
            l2_hdr_len = 18;
        }
        if (ethertype != 0x0800) return ""; // not IPv4
    } else if (link_type == DLT_NULL) {
        // BSD loopback
        if (pkt_len < 4) return "";
        uint32_t family = *reinterpret_cast<const uint32_t*>(pkt);
        if (family != 2) return ""; // not AF_INET
        l2_hdr_len = 4;
    } else {
        // Unsupported link type
        return "";
    }

    const size_t MIN_IP_TCP = 20 + 20;
    if (pkt_len < l2_hdr_len + MIN_IP_TCP + 5) {
        return "";
    }

    size_t ip_offset = l2_hdr_len;
    size_t ip_hdr_len = (pkt[ip_offset] & 0x0F) * 4;
    uint8_t protocol  = pkt[ip_offset + 9];
    if (protocol != 6) {
        return ""; // not TCP
    }

    size_t tcp_offset = ip_offset + ip_hdr_len;
    if (pkt_len < tcp_offset + 20) {
        return "";
    }
    size_t tcp_hdr_len = ((pkt[tcp_offset + 12] >> 4) & 0x0F) * 4;

    size_t tls_offset = tcp_offset + tcp_hdr_len;
    if (pkt_len < tls_offset + 5) {
        return "";
    }

    uint8_t content_type = pkt[tls_offset];
    if (content_type != 0x16) {
        return ""; // not a Handshake record
    }

    size_t handshake_offset = tls_offset + 5;
    if (pkt_len < handshake_offset + 4) {
        return "";
    }

    uint8_t handshake_type = pkt[handshake_offset];
    if (handshake_type != 0x01) {
        return ""; // not Client Hello
    }

    size_t client_hello_offset = handshake_offset + 4;
    if (pkt_len < client_hello_offset + 2 + 32) {
        return "";
    }

    size_t pos = client_hello_offset;

    // Skip Client Version (2 bytes) + Random (32 bytes)
    pos += 34;

    // Session ID
    if (pkt_len < pos + 1) return "";
    uint8_t session_id_len = pkt[pos];
    pos += 1 + session_id_len;

    // Cipher Suites
    if (pkt_len < pos + 2) return "";
    uint16_t cipher_suites_len = read_u16_be(pkt + pos);
    pos += 2 + cipher_suites_len;

    // Compression Methods
    if (pkt_len < pos + 1) return "";
    uint8_t compression_methods_len = pkt[pos];
    pos += 1 + compression_methods_len;

    // Extensions
    if (pkt_len < pos + 2) return "";
    uint16_t extensions_len = read_u16_be(pkt + pos);
    pos += 2;

    size_t extensions_end = pos + extensions_len;
    if (extensions_end > pkt_len) {
        return "";
    }

    while (pos + 4 <= extensions_end) {
        uint16_t ext_type = read_u16_be(pkt + pos);
        uint16_t ext_len  = read_u16_be(pkt + pos + 2);
        pos += 4;

        if (ext_type == 0x0000 && pos + ext_len <= extensions_end) {
            // ---- SNI Extension ----
            if (ext_len < 2) break;
            uint16_t sni_list_len = read_u16_be(pkt + pos);
            size_t sni_pos = pos + 2;
            size_t sni_list_end = pos + sni_list_len;

            if (sni_list_end > extensions_end) break;

            while (sni_pos + 3 <= sni_list_end) {
                uint8_t name_type = pkt[sni_pos];
                uint16_t name_len = read_u16_be(pkt + sni_pos + 1);
                sni_pos += 3;

                if (name_type == 0x00 && sni_pos + name_len <= sni_list_end) {
                    std::string result(reinterpret_cast<const char*>(pkt + sni_pos), name_len);
                    return trim_nulls(result);
                }
                sni_pos += name_len;
            }
            break;
        }

        pos += ext_len;
    }

    return "";
}

// Quick check if a packet is TCP destination port 443.
// Used to avoid regex fallback on encrypted traffic.
inline bool is_tcp_dst_port_443(const uint8_t* pkt, size_t pkt_len, int link_type) {
    size_t l2_hdr_len = 0;
    if (link_type == DLT_EN10MB) {
        if (pkt_len < 14) return false;
        uint16_t ethertype = read_u16_be(pkt + 12);
        l2_hdr_len = 14;
        if (ethertype == 0x8100) {
            if (pkt_len < 18) return false;
            ethertype = read_u16_be(pkt + 16);
            l2_hdr_len = 18;
        }
        if (ethertype != 0x0800) return false;
    } else if (link_type == DLT_NULL) {
        if (pkt_len < 4) return false;
        uint32_t family = *reinterpret_cast<const uint32_t*>(pkt);
        if (family != 2) return false;
        l2_hdr_len = 4;
    } else {
        return false;
    }

    if (pkt_len < l2_hdr_len + 20 + 4) return false;
    size_t ip_hdr_len = (pkt[l2_hdr_len] & 0x0F) * 4;
    size_t tcp_offset = l2_hdr_len + ip_hdr_len;
    if (pkt_len < tcp_offset + 4) return false;
    uint16_t dst_port = read_u16_be(pkt + tcp_offset + 2);
    return dst_port == 443;
}

#endif // TLS_PARSER_HPP
