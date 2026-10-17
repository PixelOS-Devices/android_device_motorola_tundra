/*
 * SPDX-FileCopyrightText: 2026 The PixelOS Project
 * SPDX-License-Identifier: Apache-2.0
 */

#include <aidl/android/hardware/power/Mode.h>
#include <android-base/logging.h>

#include <iterator>
#include <mutex>

#include "utils.h"

using ::aidl::android::hardware::power::Mode;

namespace {

// perf HAL max-freq opcodes per logical cluster, values in MHz. Perfd rounds
// them up to the next available frequency.
constexpr int kMaxFreqGold = 0x40804000;   // cpu4-6
constexpr int kMaxFreqPrime = 0x40804200;  // cpu7

// kgsl max_pwrlevel, an index into the speed bin 0 table (0 = 840 MHz).
constexpr int kGpuMaxPwrlevel = 0x42808000;
constexpr int kGpuPwrlevel491Mhz = 6;

// Stock 60 fps tuning (perfboostsconfig hint 0x1094): no WALT predictive load
// and a 5 tick WALT window.
constexpr int kSchedutilPlGold = 0x41444000;
constexpr int kSchedutilPlLittle = 0x41444100;
constexpr int kSchedutilPlPrime = 0x41444200;
constexpr int kSchedWindowTicks = 0x40CA4000;

int kLowPowerResources[] = {
        kMaxFreqGold,       1670,
        kMaxFreqPrime,      1785,
        kGpuMaxPwrlevel,    kGpuPwrlevel491Mhz,
        kSchedutilPlGold,   0,
        kSchedutilPlLittle, 0,
        kSchedutilPlPrime,  0,
        kSchedWindowTicks,  5,
};

std::mutex gLock;
int gLowPowerHandle = 0;

void setLowPower(bool enabled) {
    std::lock_guard<std::mutex> lock(gLock);
    if (enabled) {
        if (gLowPowerHandle > 0) return;
        // A duration of 0 holds the lock until it is released.
        gLowPowerHandle = interaction_with_handle(0, 0, std::size(kLowPowerResources),
                                                  kLowPowerResources);
        if (gLowPowerHandle <= 0) {
            LOG(ERROR) << "Failed to acquire low power perf lock";
            gLowPowerHandle = 0;
        }
    } else if (gLowPowerHandle > 0) {
        release_request(gLowPowerHandle);
        gLowPowerHandle = 0;
    }
}

}  // namespace

namespace aidl {
namespace android {
namespace hardware {
namespace power {
namespace impl {

bool isDeviceSpecificModeSupported(Mode type, bool* _aidl_return) {
    switch (type) {
        case Mode::LOW_POWER:
            *_aidl_return = true;
            return true;
        default:
            return false;
    }
}

bool setDeviceSpecificMode(Mode type, bool enabled) {
    switch (type) {
        case Mode::LOW_POWER:
            setLowPower(enabled);
            return true;
        default:
            return false;
    }
}

}  // namespace impl
}  // namespace power
}  // namespace hardware
}  // namespace android
}  // namespace aidl
