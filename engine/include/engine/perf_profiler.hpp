#pragma once

#ifdef _DEBUG
#include <chrono>
#include <string>
#include <unordered_map>
#include <vector>
#include <algorithm>
#include <numeric>
#include <mutex>

class PerfProfiler {
public:
    using clock = std::chrono::high_resolution_clock;
    struct Section {
        std::vector<double> samples;
        size_t maxSamples = 120;
        clock::time_point start;
        double last = 0;

        void begin() { start = clock::now(); }

        void end() {
            auto t = std::chrono::duration<double, std::milli>(clock::now() - start).count();
            last = t;
            samples.push_back(t);
            if (samples.size() > maxSamples) samples.erase(samples.begin());
        }

        double avg() const {
            if (samples.empty()) return 0;
            return std::accumulate(samples.begin(), samples.end(), 0.0) / samples.size();
        }

        double min() const {
            if (samples.empty()) return 0;
            return *std::min_element(samples.begin(), samples.end());
        }

        double max() const {
            if (samples.empty()) return 0;
            return *std::max_element(samples.begin(), samples.end());
        }

        double p99() const {
            if (samples.empty()) return 0;
            std::vector<double> sorted = samples;
            std::sort(sorted.begin(), sorted.end());
            return sorted[static_cast<size_t>(sorted.size() * 0.99)];
        }
    };

    void begin(const std::string& name) {
        std::lock_guard lock(mutex);
        sections[name].begin();
    }

    void end(const std::string& name) {
        std::lock_guard lock(mutex);
        sections[name].end();
    }

    const std::unordered_map<std::string, Section>& getSections() const { return sections; }

private:
    std::unordered_map<std::string, Section> sections;
    mutable std::mutex mutex;
};

// Global instance (optional)
extern PerfProfiler gProfiler;

// Macros for convenience
#define PERF_BEGIN(name) gProfiler.begin(name)
#define PERF_END(name)   gProfiler.end(name)

#else
#define PERF_BEGIN(name)
#define PERF_END(name)
#endif // _DEBUG
