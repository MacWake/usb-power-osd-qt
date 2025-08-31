// MeasurementHistory.h
#pragma once
#include <vector>
#include <algorithm>
#include <cstddef>
#include <stdexcept>
#include <limits>
#include "PowerData.h"

/**
 * @brief Circular buffer for storing PowerData measurements with fixed capacity.
 * 
 * Provides efficient O(1) insertion and various statistical operations on the last N samples.
 * Designed specifically for CurrentGraph widget integration with newest-first ordering.
 */
class MeasurementHistory {
public:
    /**
     * @brief Constructs history buffer with specified capacity
     * @param capacity Maximum number of measurements to store (must be > 0)
     * @throws std::invalid_argument if capacity is 0
     */
    explicit MeasurementHistory(std::size_t capacity);

    // Basic operations
    void reset() noexcept;
    void setCapacity(std::size_t newCapacity);
    void push(const PowerData& sample) noexcept;

    // Query methods
    [[nodiscard]] std::size_t capacity() const noexcept { return _size; }
    [[nodiscard]] std::size_t size() const noexcept { return _valid_count; }
    [[nodiscard]] bool is_empty() const noexcept { return _valid_count == 0; }
    [[nodiscard]] bool is_full() const noexcept { return _valid_count == _size; }

    // Statistical operations on last N samples
    bool minMaxCurrentLastN(std::size_t lastN, double& outMin, double& outMax) const noexcept;
    [[nodiscard]] bool maxValuesLastN(std::size_t lastN, double& maxVoltage, double& maxCurrent, double& maxPower) const noexcept;
    [[nodiscard]] bool medianValuesLastN(std::size_t lastN, double& medianVoltage, double& medianCurrent, double& medianPower) const noexcept;

    // Data access for CurrentGraph widget
    [[nodiscard]] std::vector<PowerData> rawHistoryNewestFirst() const;
    [[nodiscard]] std::vector<PowerData> lastNSamplesNewestFirst(std::size_t lastN) const;

    // Optional: single sample access by age (0 = newest, size()-1 = oldest)
    [[nodiscard]] const PowerData& atByAge(std::size_t ageFromNewest) const;

  private:
    // Circular buffer helpers
    [[nodiscard]] std::size_t inc(std::size_t idx) const noexcept;
    [[nodiscard]] std::size_t dec(std::size_t idx) const noexcept;
    [[nodiscard]] std::size_t newestIndex() const noexcept;
    [[nodiscard]] std::size_t oldestIndex() const noexcept;

    // Helper for median calculation
    template<typename ValueExtractor>
    [[nodiscard]] double calculateMedianLastN(std::size_t lastN, ValueExtractor extractor) const noexcept;

    std::size_t _size;
    std::vector<PowerData> _values;
    std::size_t _valid_count = 0;  // number of valid samples
    std::size_t _head = 0;  // next write position (newest+1)
};
