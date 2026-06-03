#pragma once
#ifndef STATS_HPP
#define STATS_HPP

#include <atomic>

struct ScanStats {
    std::atomic<int> total{0};
    std::atomic<int> suspicious{0};
    std::atomic<int> blocked{0};
    std::atomic<int> allowed{0};
};

#endif // STATS_HPP
