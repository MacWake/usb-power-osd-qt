// MeasurementHistory.cpp
#include "MeasurementHistory.h"
#include <algorithm>
#include <numeric>

MeasurementHistory::MeasurementHistory(std::size_t capacity)
    : _size(capacity), _values(capacity)
{
    if (_size == 0) {
        throw std::invalid_argument("MeasurementHistory capacity must be > 0");
    }
}

void MeasurementHistory::reset() noexcept {
    _valid_count = 0;
    _head = 0;
}

void MeasurementHistory::setCapacity(std::size_t newCapacity) {
    if (newCapacity == 0) {
        throw std::invalid_argument("MeasurementHistory capacity must be > 0");
    }
    _size = newCapacity;
    _values.assign(_size, PowerData{});
    reset();
}

void MeasurementHistory::push(const PowerData& sample) noexcept {
    _values[_head] = sample;
    _head = inc(_head);
    if (_valid_count < _size) {
        ++_valid_count;
    }
}

bool MeasurementHistory::minMaxCurrentLastN(std::size_t lastN, double& outMin, double& outMax) const noexcept {
    if (_valid_count == 0 || lastN == 0) {
        return false;
    }
    lastN = std::min(lastN, _valid_count);

    outMin = std::numeric_limits<double>::infinity();
    outMax = -std::numeric_limits<double>::infinity();

    std::size_t idx = newestIndex();
    for (std::size_t i = 0; i < lastN; ++i) {
        const double current = _values[idx].current;
        if (current < outMin) outMin = current;
        if (current > outMax) outMax = current;
        idx = dec(idx);
    }
    return true;
}

bool MeasurementHistory::maxValuesLastN(std::size_t lastN, double& maxVoltage, double& maxCurrent, double& maxPower) const noexcept {
    if (_valid_count == 0 || lastN == 0) {
        return false;
    }
    lastN = std::min(lastN, _valid_count);

    maxVoltage = maxCurrent = maxPower = -std::numeric_limits<double>::infinity();

    std::size_t idx = newestIndex();
    for (std::size_t i = 0; i < lastN; ++i) {
        const PowerData& data = _values[idx];
        if (data.voltage > maxVoltage) maxVoltage = data.voltage;
        if (data.current > maxCurrent) maxCurrent = data.current;
        if (data.power > maxPower) maxPower = data.power;
        idx = dec(idx);
    }
    return true;
}

bool MeasurementHistory::medianValuesLastN(std::size_t lastN, double& medianVoltage, double& medianCurrent, double& medianPower) const noexcept {
    if (_valid_count == 0 || lastN == 0) {
        return false;
    }
    lastN = std::min(lastN, _valid_count);

    medianVoltage = calculateMedianLastN(lastN, [](const PowerData& data) { return data.voltage; });
    medianCurrent = calculateMedianLastN(lastN, [](const PowerData& data) { return data.current; });
    medianPower = calculateMedianLastN(lastN, [](const PowerData& data) { return data.power; });
    
    return true;
}

std::vector<PowerData> MeasurementHistory::rawHistoryNewestFirst() const {
    std::vector<PowerData> out;
    out.reserve(_valid_count);
    if (_valid_count == 0) return out;

    std::size_t idx = newestIndex();
    for (std::size_t i = 0; i < _valid_count; ++i) {
        out.push_back(_values[idx]);
        idx = dec(idx);
    }
    return out;
}

std::vector<PowerData> MeasurementHistory::lastNSamplesNewestFirst(std::size_t lastN) const {
    if (_valid_count == 0 || lastN == 0) return {};
    lastN = std::min(lastN, _valid_count);

    std::vector<PowerData> out;
    out.reserve(lastN);
    
    std::size_t idx = newestIndex();
    for (std::size_t i = 0; i < lastN; ++i) {
        out.push_back(_values[idx]);
        idx = dec(idx);
    }
    return out;
}

const PowerData& MeasurementHistory::atByAge(std::size_t ageFromNewest) const {
    if (ageFromNewest >= _valid_count) {
        throw std::out_of_range("MeasurementHistory::atByAge out of range");
    }
    
    std::size_t idx = newestIndex();
    for (std::size_t i = 0; i < ageFromNewest; ++i) {
        idx = dec(idx);
    }
    return _values[idx];
}

// Private helper methods
std::size_t MeasurementHistory::inc(std::size_t idx) const noexcept {
    return (idx + 1) % _size;
}

std::size_t MeasurementHistory::dec(std::size_t idx) const noexcept {
    return (idx + _size - 1) % _size;
}

std::size_t MeasurementHistory::newestIndex() const noexcept {
    return (_head + _size - 1) % _size;
}

std::size_t MeasurementHistory::oldestIndex() const noexcept {
    return (_valid_count == _size) ? _head : 0;
}

template<typename ValueExtractor>
double MeasurementHistory::calculateMedianLastN(std::size_t lastN, ValueExtractor extractor) const noexcept {
    std::vector<double> values;
    values.reserve(lastN);
    
    std::size_t idx = newestIndex();
    for (std::size_t i = 0; i < lastN; ++i) {
        values.push_back(extractor(_values[idx]));
        idx = dec(idx);
    }
    
    std::sort(values.begin(), values.end());
    
    if (values.size() % 2 == 0) {
        // Even number of elements: average of middle two
        const std::size_t mid = values.size() / 2;
        return (values[mid - 1] + values[mid]) / 2.0;
    } else {
        // Odd number of elements: middle element
        return values[values.size() / 2];
    }
}
