// coop/voice/radio_state.cpp -- see radio_state.h.

#include "coop/voice/radio_state.h"

#include <atomic>

namespace coop::radio_state {
namespace {

std::atomic<bool> g_powered{false};
std::atomic<bool> g_held{false};
std::atomic<bool> g_heldPowered{false};
std::atomic<float> g_interference{0.0f};

}  // namespace

void SetPowered(bool powered) {
    g_powered.store(powered, std::memory_order_release);
}

void SetHeld(bool held) {
    g_held.store(held, std::memory_order_release);
}

void SetHeldPowered(bool powered) {
    g_heldPowered.store(powered, std::memory_order_release);
}

bool Powered() {
    return g_powered.load(std::memory_order_acquire);
}

bool Held() {
    return g_held.load(std::memory_order_acquire);
}

bool HeldPowered() {
    return g_heldPowered.load(std::memory_order_acquire);
}

bool CanTransmit() {
    return Held() && HeldPowered();
}

void SetInterference(float level) {
    if (level < 0.0f) level = 0.0f;
    if (level > 1.0f) level = 1.0f;
    g_interference.store(level, std::memory_order_release);
}

float Interference() {
    return g_interference.load(std::memory_order_acquire);
}

void Reset() {
    g_interference.store(0.0f, std::memory_order_release);
    g_held.store(false, std::memory_order_release);
    g_powered.store(false, std::memory_order_release);
}

}  // namespace coop::radio_state
