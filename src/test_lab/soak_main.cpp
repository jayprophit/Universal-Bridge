#include "ubridge/core/midi_mediation.hpp"
#include "ubridge/core/realtime_buffer.hpp"

#include <atomic>
#include <chrono>
#include <cstdint>
#include <iostream>
#include <limits>
#include <thread>

namespace {

constexpr std::uint64_t kQueueIterations = 2'000'000;
constexpr std::uint64_t kMidiIterations = 250'000;

bool run_concurrent_queue_stress() {
    ubridge::core::SpscRingBuffer<std::uint64_t, 4096> queue;
    std::atomic<bool> producer_done{false};
    std::atomic<bool> ordering_failure{false};

    std::thread producer([&] {
        for (std::uint64_t value = 1; value <= kQueueIterations; ++value) {
            while (!queue.try_push(value)) {
                std::this_thread::yield();
            }
        }
        producer_done.store(true, std::memory_order_release);
    });

    std::uint64_t expected = 1;
    while (!producer_done.load(std::memory_order_acquire) || !queue.empty()) {
        std::uint64_t value = 0;
        if (!queue.try_pop(value)) {
            std::this_thread::yield();
            continue;
        }
        if (value != expected) {
            ordering_failure.store(true, std::memory_order_relaxed);
            break;
        }
        ++expected;
    }
    producer.join();
    return !ordering_failure.load(std::memory_order_relaxed) && expected == kQueueIterations + 1 && queue.empty();
}

bool run_midi_mediation_stress() {
    ubridge::core::MidiMediationPolicy policy;
    policy.transport_authority = ubridge::core::MidiRouteEndpoint::daw;
    policy.clock_authority = ubridge::core::MidiRouteEndpoint::daw;
    policy.maximum_messages_per_window = std::numeric_limits<std::uint32_t>::max();
    policy.rate_window_microseconds = 1;
    policy.echo_window_microseconds = 0;
    ubridge::core::MidiMediator mediator(policy);

    ubridge::core::RoutedMidiPacket routed;
    for (std::uint64_t sequence = 1; sequence <= kMidiIterations; ++sequence) {
        ubridge::core::ShortMidiPacket packet;
        packet.bytes = {static_cast<std::uint8_t>(0x90U | (sequence & 0x0FU)),
                        static_cast<std::uint8_t>(sequence % 128U), static_cast<std::uint8_t>((sequence * 17U) % 128U)};
        packet.size = 3;
        packet.timestamp_microseconds = sequence;
        packet.sequence = sequence;
        const auto source =
            (sequence & 1U) == 0U ? ubridge::core::MidiRouteEndpoint::hardware : ubridge::core::MidiRouteEndpoint::daw;
        if (mediator.route(source, packet) != ubridge::core::MidiRouteDecision::forwarded) {
            return false;
        }
        const bool popped = source == ubridge::core::MidiRouteEndpoint::hardware ? mediator.pop_for_daw(routed)
                                                                                 : mediator.pop_for_hardware(routed);
        if (!popped || routed.packet.sequence != sequence || routed.source != source) {
            return false;
        }
    }
    return true;
}

} // namespace

int main() {
    const auto started = std::chrono::steady_clock::now();
    if (!run_concurrent_queue_stress()) {
        std::cerr << "concurrent SPSC queue ordering stress failed\n";
        return 1;
    }
    if (!run_midi_mediation_stress()) {
        std::cerr << "MIDI mediation routing stress failed\n";
        return 2;
    }
    const auto elapsed =
        std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - started);
    std::cout << "deterministic stress passed: queue=" << kQueueIterations << " midi=" << kMidiIterations
              << " elapsed_ms=" << elapsed.count() << '\n';
    std::cout << "Boundary: synthetic stress is not a physical MIDI, DAW, or long-duration soak test\n";
    return 0;
}
