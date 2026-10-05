#pragma once

#include "ue_wrap/core/log.h"

#include <chrono>

namespace coop::dev {

// Diagnostic-only wall timer for top-level pump regions. Two steady-clock
// reads in the silent path; no allocation and no logging below the threshold.
class HitchTrace final {
public:
    explicit HitchTrace(const char* label)
        : label_(label), started_(std::chrono::steady_clock::now()) {}

    HitchTrace(const HitchTrace&) = delete;
    HitchTrace& operator=(const HitchTrace&) = delete;

    ~HitchTrace() {
        const auto elapsedUs = std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::steady_clock::now() - started_).count();
        if (elapsedUs >= 2000) {
            UE_LOGI("[HITCH-TRACE] %s = %.3f ms", label_,
                    static_cast<double>(elapsedUs) / 1000.0);
        }
    }

private:
    const char* label_;
    std::chrono::steady_clock::time_point started_;
};

}  // namespace coop::dev
