#include <cstring>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

extern "C" {
#include "ring_buf.h"
}

extern "C" {

esp_err_t ring_buf_init(ring_buf_t* rb, uint16_t size, uint16_t item_width, uint8_t* buf, bool static_buf) {
    if (rb == nullptr || size == 0 || item_width == 0) return ESP_ERR_INVALID_ARG;
    rb->size = size;
    rb->item_width = item_width;
    rb->head = 1;
    rb->_head = 1;

    if (buf == nullptr) {
        if (static_buf) return ESP_ERR_INVALID_ARG;
        buf = static_cast<uint8_t*>(malloc(size * item_width));
        if (buf == nullptr) return ESP_ERR_NO_MEM;
        rb->buf = buf;
    } else {
        if (static_buf) {
            rb->buf = buf;
        } else {
            return ESP_ERR_INVALID_ARG;
        }
    }
    rb->static_buf = static_buf;
    rb->mutex = xSemaphoreCreateMutex();
    return ESP_OK;
}

esp_err_t ring_buf_reset(ring_buf_t* rb) {
    rb->head = 1;
    rb->_head = 1;
    return ESP_OK;
}

esp_err_t ring_buf_reset_safe(ring_buf_t* rb) {
    if (xSemaphoreTake(rb->mutex, portMAX_DELAY) != pdTRUE) return ESP_FAIL;
    esp_err_t ret = ring_buf_reset(rb);
    xSemaphoreGive(rb->mutex);
    return ret;
}

esp_err_t IRAM_ATTR ring_buf_push(ring_buf_t* rb, void* item) {
    if (rb == nullptr || item == nullptr) return ESP_ERR_INVALID_ARG;
    memcpy(rb->buf + (rb->_head * rb->item_width), item, rb->item_width);
    rb->_head = (rb->_head + 1) % rb->size;
    rb->head++;
    return ESP_OK;
}

esp_err_t IRAM_ATTR ring_buf_push_safe(ring_buf_t* rb, void* item) {
    if (xSemaphoreTake(rb->mutex, portMAX_DELAY) != pdTRUE) return ESP_FAIL;
    esp_err_t ret = ring_buf_push(rb, item);
    xSemaphoreGive(rb->mutex);
    return ret;
}

esp_err_t IRAM_ATTR ring_buf_peek(ring_buf_t* rb, void* item, int64_t index, int64_t* index_out) {
    if (rb == nullptr || item == nullptr) return ESP_ERR_INVALID_ARG;
    if (index >= rb->head) return ESP_ERR_INVALID_ARG;
    if (index < 0 || index <= (rb->head - rb->size)) index = rb->head - 1;
    if (index_out != nullptr) *index_out = index;
    int64_t place = index % rb->size;
    memcpy(item, rb->buf + (place * rb->item_width), rb->item_width);
    return ESP_OK;
}

esp_err_t ring_buf_free(ring_buf_t* rb) {
    if (rb == nullptr) return ESP_ERR_INVALID_ARG;
    if (!rb->static_buf) free(rb->buf);
    vSemaphoreDelete(rb->mutex);
    return ESP_OK;
}

} // extern "C"
