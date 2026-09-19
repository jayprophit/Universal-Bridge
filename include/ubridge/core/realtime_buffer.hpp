#pragma once

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <span>
#include <type_traits>

namespace ubridge::core {

// Single-producer/single-consumer fixed-capacity queue. Construction may occur
// off the real-time thread; push/pop never allocate, lock, log, or call the OS.
template <typename T, std::size_t Capacity> class SpscRingBuffer {
    static_assert(Capacity > 1, "SPSC capacity must be greater than one");
    static_assert(std::is_trivially_copyable_v<T>, "SPSC payloads must be trivially copyable");

  public:
    [[nodiscard]] bool try_push(const T& value) noexcept {
        const auto write = write_sequence_.load(std::memory_order_relaxed);
        const auto read = read_sequence_.load(std::memory_order_acquire);
        if (write - read >= Capacity) return false;
        storage_[static_cast<std::size_t>(write % Capacity)] = value;
        write_sequence_.store(write + 1, std::memory_order_release);
        return true;
    }

    [[nodiscard]] std::size_t try_push(std::span<const T> values) noexcept {
        std::size_t count = 0;
        for (const auto& value : values) {
            if (!try_push(value)) break;
            ++count;
        }
        return count;
    }

    [[nodiscard]] bool try_pop(T& value) noexcept {
        const auto read = read_sequence_.load(std::memory_order_relaxed);
        const auto write = write_sequence_.load(std::memory_order_acquire);
        if (read == write) return false;
        value = storage_[static_cast<std::size_t>(read % Capacity)];
        read_sequence_.store(read + 1, std::memory_order_release);
        return true;
    }

    [[nodiscard]] std::size_t try_pop(std::span<T> values) noexcept {
        std::size_t count = 0;
        for (auto& value : values) {
            if (!try_pop(value)) break;
            ++count;
        }
        return count;
    }

    [[nodiscard]] std::size_t size_approx() const noexcept {
        const auto write = write_sequence_.load(std::memory_order_acquire);
        const auto read = read_sequence_.load(std::memory_order_acquire);
        const auto size = write - read;
        return static_cast<std::size_t>(size > Capacity ? Capacity : size);
    }

    [[nodiscard]] constexpr std::size_t capacity() const noexcept { return Capacity; }
    [[nodiscard]] bool empty() const noexcept { return size_approx() == 0; }

    void reset() noexcept {
        // Call only when producer and consumer are stopped.
        read_sequence_.store(0, std::memory_order_relaxed);
        write_sequence_.store(0, std::memory_order_relaxed);
    }

  private:
    std::array<T, Capacity> storage_{};
    alignas(64) std::atomic<std::uint64_t> write_sequence_{0};
    alignas(64) std::atomic<std::uint64_t> read_sequence_{0};
};

} // namespace ubridge::core
