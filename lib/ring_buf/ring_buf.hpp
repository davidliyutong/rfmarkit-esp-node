#pragma once

#include <cstring>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "esp_attr.h"

template <typename T, size_t N>
class RingBuffer {
public:
    RingBuffer() {
        mutex_ = xSemaphoreCreateMutex();
    }

    ~RingBuffer() {
        if (mutex_) vSemaphoreDelete(mutex_);
    }

    // Non-copyable
    RingBuffer(const RingBuffer&) = delete;
    RingBuffer& operator=(const RingBuffer&) = delete;

    void IRAM_ATTR push(const T& item) {
        buffer_[head_idx_] = item;
        head_idx_ = (head_idx_ + 1) % N;
        abs_head_++;
    }

    void IRAM_ATTR push_safe(const T& item) {
        if (xSemaphoreTake(mutex_, portMAX_DELAY) == pdTRUE) {
            push(item);
            xSemaphoreGive(mutex_);
        }
    }

    bool IRAM_ATTR peek(int64_t index, T& out, int64_t* index_out = nullptr) const {
        if (index >= abs_head_) return false;
        if (index < 0 || index <= (abs_head_ - static_cast<int64_t>(N))) {
            index = abs_head_ - 1;
        }
        if (index_out) *index_out = index;
        int64_t place = index % static_cast<int64_t>(N);
        out = buffer_[place];
        return true;
    }

    void reset() {
        abs_head_ = 1;
        head_idx_ = 1;
    }

    void reset_safe() {
        if (xSemaphoreTake(mutex_, portMAX_DELAY) == pdTRUE) {
            reset();
            xSemaphoreGive(mutex_);
        }
    }

    int64_t head() const { return abs_head_; }
    static constexpr size_t capacity() { return N; }

private:
    T buffer_[N] = {};
    uint16_t head_idx_ = 1;
    int64_t abs_head_ = 1;
    SemaphoreHandle_t mutex_ = nullptr;
};
