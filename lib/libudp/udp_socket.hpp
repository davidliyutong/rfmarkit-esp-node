#pragma once

#include <cstring>
#include "esp_err.h"
#include "esp_log.h"
#include "lwip/inet.h"
#include "lwip/sockets.h"

class UdpSocket {
public:
    UdpSocket() = default;

    ~UdpSocket() {
        if (fd_ >= 0) {
            close(fd_);
            fd_ = -1;
        }
    }

    // Non-copyable
    UdpSocket(const UdpSocket&) = delete;
    UdpSocket& operator=(const UdpSocket&) = delete;

    // Movable
    UdpSocket(UdpSocket&& other) noexcept
        : fd_(other.fd_), source_port_(other.source_port_), dest_port_(other.dest_port_),
          source_addr_(other.source_addr_), dest_addr_(other.dest_addr_),
          reply_addr_(other.reply_addr_), initialized_(other.initialized_) {
        other.fd_ = -1;
        other.initialized_ = false;
    }

    UdpSocket& operator=(UdpSocket&& other) noexcept {
        if (this != &other) {
            if (fd_ >= 0) close(fd_);
            fd_ = other.fd_;
            source_port_ = other.source_port_;
            dest_port_ = other.dest_port_;
            source_addr_ = other.source_addr_;
            dest_addr_ = other.dest_addr_;
            reply_addr_ = other.reply_addr_;
            initialized_ = other.initialized_;
            other.fd_ = -1;
            other.initialized_ = false;
        }
        return *this;
    }

    esp_err_t init(uint16_t source_port, const char* dest_addr, uint16_t dest_port) {
        if (initialized_) return ESP_FAIL;

        fd_ = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
        if (fd_ < 0) return ESP_FAIL;
        initialized_ = true;

        source_addr_.sin_family = AF_INET;
        source_addr_.sin_addr.s_addr = htonl(INADDR_ANY);

        if (source_port != 0) {
            esp_err_t err = bind_port(source_port);
            if (err != ESP_OK) {
                close(fd_);
                fd_ = -1;
                initialized_ = false;
                return ESP_FAIL;
            }
        }

        set_destination(dest_addr, dest_port);

        if (strcmp(dest_addr, "255.255.255.255") == 0) {
            set_broadcast(true);
        }
        return ESP_OK;
    }

    void deinit() {
        if (fd_ >= 0) {
            close(fd_);
            fd_ = -1;
        }
        initialized_ = false;
    }

    esp_err_t bind_port(uint16_t port) {
        if (!initialized_) return ESP_FAIL;
        source_port_ = port;
        source_addr_.sin_port = htons(port);
        int ret = bind(fd_, reinterpret_cast<struct sockaddr*>(&source_addr_), sizeof(struct sockaddr));
        return (ret == 0) ? ESP_OK : ESP_FAIL;
    }

    esp_err_t set_destination(const char* ip, uint16_t port) {
        if (!initialized_) return ESP_FAIL;
        dest_addr_.sin_family = AF_INET;
        dest_addr_.sin_addr.s_addr = inet_addr(ip);
        dest_port_ = port;
        dest_addr_.sin_port = htons(port);
        return ESP_OK;
    }

    esp_err_t set_broadcast(bool enable) {
        if (!initialized_) return ESP_FAIL;
        int flag = enable ? 1 : 0;
        int ret = setsockopt(fd_, SOL_SOCKET, SO_BROADCAST, &flag, sizeof(flag));
        return (ret == 0) ? ESP_OK : ESP_FAIL;
    }

    esp_err_t set_timeout(uint32_t timeout_s) {
        if (!initialized_) return ESP_FAIL;
        struct timeval timeout = {static_cast<long>(timeout_s), 0};
        int ret = setsockopt(fd_, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
        return (ret == 0) ? ESP_OK : ESP_FAIL;
    }

    esp_err_t send(const void* data, size_t len) {
        if (!initialized_) return ESP_FAIL;
        ssize_t ret = sendto(fd_, data, len, 0,
                             reinterpret_cast<const struct sockaddr*>(&dest_addr_),
                             sizeof(struct sockaddr));
        return (ret >= 0) ? ESP_OK : ESP_ERR_NO_MEM;
    }

    esp_err_t recv(void* buf, size_t buf_size, size_t& out_len) {
        if (!initialized_) return ESP_FAIL;
        socklen_t socklen = sizeof(reply_addr_);
        ssize_t ret = recvfrom(fd_, buf, buf_size, 0,
                               reinterpret_cast<struct sockaddr*>(&reply_addr_), &socklen);
        out_len = (ret >= 0) ? static_cast<size_t>(ret) : 0;
        return (ret >= 0) ? ESP_OK : ESP_FAIL;
    }

    bool is_initialized() const { return initialized_; }
    int fd() const { return fd_; }
    const struct sockaddr_in& reply_address() const { return reply_addr_; }

private:
    int fd_ = -1;
    uint16_t source_port_ = 0;
    uint16_t dest_port_ = 0;
    struct sockaddr_in source_addr_ = {};
    struct sockaddr_in dest_addr_ = {};
    struct sockaddr_in reply_addr_ = {};
    bool initialized_ = false;
};
