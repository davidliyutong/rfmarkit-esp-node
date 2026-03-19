# 10 - 底层库

## 环形缓冲区（ring_buf）

**路径**: `lib/ring_buf/`

### 用途

IMU 数据采集任务与网络发送任务之间的生产者-消费者数据管道。

### 设计特点

- 固定大小的 FIFO 队列
- Mutex 保护的线程安全操作
- 支持多个独立读取者（多播友好）
- 静态或动态内存分配

### 容量

`CONFIG_DATA_BUF_LEN` = 128 帧，每帧为 `imu_dgram_t` 大小。

### API

```c
// 初始化（静态或动态分配）
esp_err_t ring_buf_init(ring_buf_t *rb, size_t capacity, size_t item_size);

// 写入一帧（生产者调用）
esp_err_t ring_buf_push(ring_buf_t *rb, const void *item);

// 读取指定索引的帧（消费者调用，非破坏性）
esp_err_t ring_buf_peek(ring_buf_t *rb, size_t index, void *out_item);

// 清空缓冲区
void ring_buf_reset(ring_buf_t *rb);
```

### 数据流

```
app_monitor (生产者)
    │
    push(imu_dgram_t)    ← Mutex 保护
    │
    ▼
┌─────────────────────────────┐
│  [0] [1] [2] ... [127]     │  环形缓冲区
│   ↑write_idx    ↑read_idx  │
└─────────────────────────────┘
    │
    peek(index)           ← Mutex 保护
    │
    ▼
app_data_client (消费者)
```

## UDP 套接字库（libudp）

**路径**: `lib/libudp/`

### 用途

封装 LWIP UDP 套接字操作，提供简化的发送/接收接口。

### API

```c
// 创建 UDP 套接字
esp_err_t udp_socket_init(udp_socket_t *sock,
                          const char *src_addr, uint16_t src_port,
                          const char *dst_addr, uint16_t dst_port);

// 启用广播模式
esp_err_t udp_socket_set_broadcast(udp_socket_t *sock, bool enable);

// 发送数据
int udp_socket_send(udp_socket_t *sock, const void *data, size_t len);

// 接收数据（带超时）
int udp_socket_recv(udp_socket_t *sock, void *buf, size_t len, int timeout_ms);

// 关闭套接字
void udp_socket_close(udp_socket_t *sock);
```

### 使用场景

| 服务 | 源端口 | 目标端口 | 模式 |
|------|--------|---------|------|
| IMU 数据流 | — | 18891 | 广播 |
| 设备发现 | — | 18889 | 广播 |

## TCP 套接字库（libtcp）

**路径**: `lib/libtcp/`

### 状态

预留实现，当前未集成到任何组件。提供基本的 TCP 客户端/服务器套接字封装。

## 空间数学库（spatial）

**路径**: `lib/spatial/`

### 用途

3D 旋转和四元数运算，用于 IMU 数据处理和 REST API 响应中的姿态转换。

### API

```c
// 四元数 → 欧拉角
void spatial_quaternion_to_euler(const float quat[4], float euler[3]);

// 四元数 → 3×3 旋转矩阵
void spatial_quaternion_to_rotation_matrix(const float quat[4], float mat[9]);

// 旋转矩阵 → 四元数
void spatial_rotation_matrix_to_quaternion(const float mat[9], float quat[4]);

// 向量缩放相加: result = a + scale * b
void spatial_vector_multiply_plus(const float *a, const float *b,
                                   float scale, float *result, int dim);

// 向量模长
float spatial_vector_norm(const float *v, int dim);
```

### 数学说明

**四元数格式**: `(w, x, y, z)`，其中 `w` 为标量部分

**欧拉角**: `(roll, pitch, yaw)`，单位为弧度

**旋转矩阵**: 3×3 行优先存储

## 依赖关系图

```
┌────────────────────────┐
│   应用层 (apps, sys)     │
├────────────┬───────────┤
│  ring_buf  │  libudp   │  ← 数据管道和网络
├────────────┼───────────┤
│  spatial   │  battery  │  ← 数学运算和硬件
├────────────┼───────────┤
│  libtcp    │           │  ← 预留
├────────────┴───────────┤
│  ESP-IDF (FreeRTOS,    │
│  LWIP, driver, ADC)    │
└────────────────────────┘
```

## 编译集成

所有库通过顶层 `CMakeLists.txt` 注册为 ESP-IDF 额外组件：

```cmake
set(EXTRA_COMPONENT_DIRS
    lib/bno08x  lib/hi229  lib/battery
    lib/ring_buf  lib/libudp  lib/libtcp  lib/spatial
    components/)
```

每个库有独立的 `CMakeLists.txt` 声明其源文件和依赖关系。
