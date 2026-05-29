# 和面机 WiFi 模块技术文档

**版本**: v1.0 | **芯片**: ESP32-S3 | **协议栈**: ESP-IDF v5.5.4 + lwIP

---

## 目录

1. [架构概览](#1-架构概览)
2. [WiFi AP 热点](#2-wifi-ap-热点)
3. [TCP 指令服务器](#3-tcp-指令服务器)
4. [指令系统](#4-指令系统)
5. [定时任务机制](#5-定时任务机制)
6. [数据流图](#6-数据流图)
7. [错误处理](#7-错误处理)
8. [配置参考](#8-配置参考)
9. [调试指南](#9-调试指南)

---

## 1. 架构概览

```
┌──────────────────────────────────────────┐
│                   手机 / 电脑              │
│  ┌─────────────┐  ┌───────────────────┐   │
│  │ 浏览器 :80   │  │ TCP 客户端 :8080  │   │
│  └──────┬──────┘  └────────┬──────────┘   │
└─────────┼──────────────────┼──────────────┘
          │ HTTP             │ TCP (纯文本)
          ▼                  ▼
┌─────────────────────────────────────────┐
│              ESP32-S3                    │
│  ┌──────────┐  ┌────────────────────┐   │
│  │ HTTP 服务 │  │  TCP 指令服务器     │   │
│  │ (http.c) │  │  (wifi.c:429)      │   │
│  └────┬─────┘  └─────────┬──────────┘   │
│       │                  │               │
│       ▼                  ▼               │
│  ┌──────────────────────────────┐        │
│  │        电机控制层 (motor.c)   │        │
│  └──────────────────────────────┘        │
└─────────────────────────────────────────┘
```

WiFi 模块提供两种网络接入方式:

| 服务 | 端口 | 协议 | 用途 |
|------|------|------|------|
| HTTP | 80 | HTTP/1.1 | 网页控制面板 + REST API |
| TCP | 8080 | TCP Raw | 文本指令调试 |

---

## 2. WiFi AP 热点

### 2.1 启动流程

`wifi_init_softap()` 按 ESP-IDF 要求的严格顺序初始化 (wifi.c:363-405):

```
步骤 1  esp_netif_init()
        └─ 初始化 lwIP TCP/IP 协议栈

步骤 2  esp_event_loop_create_default()
        └─ 创建系统事件循环 (WiFi 驱动发送事件到此循环)
        └─ 幂等操作: 重复调用不会出错

步骤 3  esp_netif_create_default_wifi_ap()
        └─ 创建 AP 网络接口
        └─ 自动启动 DHCP 服务器 (地址池 192.168.4.2 ~ 192.168.4.254)

步骤 4  esp_wifi_init(&cfg)
        └─ 分配 WiFi 驱动内存, 创建 WiFi 内核任务

步骤 5  esp_event_handler_instance_register()
        └─ 注册 STA 连接/断开事件回调

步骤 6  esp_wifi_set_mode(WIFI_MODE_AP)
        └─ 设为纯 AP 模式 (不扫描其他 WiFi)

步骤 7  esp_wifi_set_config() + esp_wifi_start()
        └─ 配置 SSID/密码/信道, 启动射频
```

### 2.2 配置参数

| 参数 | 值 | 宏定义 | 说明 |
|------|-----|--------|------|
| SSID | `dough_mixer` | `WIFI_SSID` | WiFi 名称 |
| 密码 | `12345678` | `WIFI_PASS` | WPA2-PSK 加密 |
| 信道 | 1 | `WIFI_CHANNEL` | 2.4GHz, 2412MHz |
| 最大连接 | 4 | `MAX_STA_CONN` | 同时连接的客户端数 |
| PMF | 关闭 | `.required = false` | 兼容旧手机 |
| IP | `192.168.4.1` | — | 网关/DHCP 服务器 |
| 子网掩码 | `255.255.255.0` | — | C 类网络 |

### 2.3 事件处理

`wifi_event_handler()` (wifi.c:329-341) 监听两类事件:

| 事件 | 触发时机 | 处理 |
|------|---------|------|
| `WIFI_EVENT_AP_STACONNECTED` | 客户端连接 | 打印 MAC + AID |
| `WIFI_EVENT_AP_STADISCONNECTED` | 客户端断开 | 打印 MAC + AID |

---

## 3. TCP 指令服务器

### 3.1 生命周期

`tcp_server_task()` (wifi.c:429-520) 是永不退出的 FreeRTOS 任务, 完整的 Socket 生命周期:

```
┌─────────────────────────────────────────────────┐
│ 1. socket(AF_INET, SOCK_STREAM, IPPROTO_IP)    │  创建 IPv4 TCP socket
│ 2. setsockopt(SO_REUSEADDR, 1)                  │  允许端口复用 (快速重启)
│ 3. bind(0.0.0.0:8080)                           │  绑定所有网络接口
│ 4. listen(backlog=1)                            │  开始监听
│                                                 │
│ ┌─── while(1) ───────────────────────────────┐ │
│ │ 5. accept()          阻塞等待客户端连接      │ │
│ │ 6. recv() 循环        逐行接收指令           │ │
│ │    └─ wifi_process_cmd()  解析并执行         │ │
│ │ 7. shutdown() + close() 客户端断开后清理     │ │
│ │    └─ 回到步骤 5                             │ │
│ └─────────────────────────────────────────────┘ │
└─────────────────────────────────────────────────┘
```

### 3.2 关键设计决策

| 决策 | 实现 | 原因 |
|------|------|------|
| 单客户端模式 | `listen(backlog=1)` | 简化并发控制, 避免多客户端指令冲突 |
| 端口复用 | `SO_REUSEADDR` | 重启后立即 bind, 避免 TIME_WAIT 等待 |
| 接收缓冲区 | 127 字节 | 单行指令不超过此长度 |
| 任务栈 | 4096 字节 | `accept()` + `recv()` + lwIP 内部调用 |
| 任务优先级 | 5 | 与 LVGL 同级, 高于电机任务 (5) |

### 3.3 连接方式

```bash
# 电脑 (Linux/Mac/Windows WSL)
nc 192.168.4.1 8080

# Windows (Putty)
# 选择 Raw TCP 模式, 主机 192.168.4.1, 端口 8080

# Android
# 安装 "TCP Client" 等 App

# iOS
# 安装 "TCP Telnet" / "Mocha Telnet" 等 App
```

---

## 4. 指令系统

### 4.1 通用规则

- **大小写不敏感**: 所有指令使用 `strcasecmp`/`strncasecmp` 比较
- **回车结束**: 每条指令以 `\r` 或 `\n` 结尾
- **响应格式**: 每条指令执行后返回一行文本 (以 `\r\n` 结尾)
- **空白忽略**: 首尾空格自动去除
- **空行忽略**: 长度为零的输入静默丢弃

### 4.2 指令解析流程

`wifi_process_cmd()` (wifi.c:172-317):

```
输入文本行
  ├─ 去除首尾空白
  ├─ 长度为零 → 忽略
  │
  ├─ "STOP" ──────────────→ motor_emergency_stop()
  │
  ├─ "SS A <angle>" ─────→ steering_servo_set_angle()
  ├─ "SP A <angle>" ─────→ push_servo_set_angle()
  │
  ├─ "M A <angle>" ──────→ step_rotate_angle(CW)
  ├─ "M B <angle>" ──────→ step_rotate_angle(CCW)
  ├─ "M F <turns>" ──────→ step_rotate_turns(CW)
  ├─ "M R <turns>" ──────→ step_rotate_turns(CCW)
  ├─ "M START" ──────────→ step_start_auto_mode()
  ├─ "M STOP" ───────────→ step_stop_auto_mode()
  │
  ├─ "P <seconds>" ──────→ wifi_start_timed_task('P')
  ├─ "D <seconds>" ──────→ wifi_start_timed_task('D')
  ├─ "MI <minutes>" ─────→ wifi_start_timed_task('M')
  ├─ "G <seconds>" ──────→ wifi_start_timed_task('G')
  │
  ├─ "H" ────────────────→ motor_coordinated_control()
  │
  └─ 其他 ───────────────→ "Error: Unknown TCP command"
```

### 4.3 指令分类

**即时指令** — 在 TCP 连接的上下文中直接执行, 阻塞响应:

| 指令 | 参数类型 | 范围 | 示例 | 阻塞时间 |
|------|---------|------|------|---------|
| `M A <angle>` | float (度) | 0~360 | `M A 90` | angle × 0.225° ÷ 频率 |
| `M B <angle>` | float (度) | 0~360 | `M B 45` | 同上 |
| `M F <turns>` | float (圈) | >0 | `M F 2.5` | turns × 1.6s |
| `M R <turns>` | float (圈) | >0 | `M R 1.0` | turns × 1.6s |
| `M START` | — | — | `M START` | 立即返回 |
| `M STOP` | — | — | `M STOP` | 立即返回 |
| `SS A <angle>` | float (度) | 0~180 | `SS A 90` | <1ms |
| `SP A <angle>` | float (度) | 0~180 | `SP A 45` | <1ms |
| `STOP` | — | — | `STOP` | <10ms |

**定时指令** — 创建 FreeRTOS 任务异步执行, 立即返回:

| 指令 | 参数类型 | 单位 | 示例 | 后台运行时长 |
|------|---------|------|------|-------------|
| `P <seconds>` | int | 秒 | `P 10` | seconds 秒 |
| `D <seconds>` | int | 秒 | `D 15` | seconds 秒 |
| `G <seconds>` | int | 秒 | `G 20` | seconds 秒 |
| `MI <minutes>` | int | 分钟 | `MI 5` | minutes × 60 秒 |

**协同控制**:

| 指令 | 说明 | 示例 |
|------|------|------|
| `H` | 默认参数协同控制 (阻塞) | `H` |

### 4.4 指令交互示例

```
[客户端连接 192.168.4.1:8080]

> M F 2.0
Stepper rotated 2.0 turns: SUCCESS

> P 10
(立即返回, 水泵在后台运行 10 秒)

> D 15
(水泵任务被销毁, 面粉电机开始运行 15 秒)

> STOP
Emergency Stop All: SUCCESS

> SS A 90
Steering servo set to 90.0 deg: SUCCESS

> H
Coordinated control started: SUCCESS
(约 160 秒后完成)

> XXX
Error: Unknown TCP command
```

### 4.5 指令解析注意事项

**`MI` 前缀优先于 `M`**: 必须先匹配 "MI " (3 字符), 再匹配 "M " (2 字符). 因为 "MI 5" 以 "M" 开头, 如果先匹配 "M ", 会将 "I 5" 当作子命令解析导致失败.

**浮点数解析**: 使用 C 标准库 `atof()`, 支持小数如 `2.5`. 不支持科学计数法.

**整数解析**: 使用 `atoi()`, 负数会被截断为 0 或未定义值, 应避免传入负数.

---

## 5. 定时任务机制

### 5.1 设计目的

水泵(P)、面粉(D)、搅拌(MI)、研磨(G) 指令需要运行指定时长后自动停止. 如果在 TCP 连接上下文中直接执行, 会阻塞整个连接直到完成. 因此采用 FreeRTOS 任务异步执行.

### 5.2 任务结构

```
wifi_start_timed_task(sock, type, val)
  │
  ├─ 销毁旧任务 (如果存在)
  │   └─ vTaskDelete(old_handle)
  │   └─ motor_emergency_stop()
  │
  ├─ malloc 分配 wifi_timed_cmd_t 参数
  │   ├─ .type = 'P'|'D'|'M'|'G'
  │   ├─ .val  = 运行时长
  │   └─ .sock = 客户端 socket
  │
  └─ xTaskCreate(wifi_timed_action_task)
      │
      ├─ 发送 "starting..." 通知
      ├─ motor_start()             ← 启动电机硬件
      ├─ vTaskDelay(duration)      ← 非阻塞等待 (让出 CPU)
      ├─ motor_stop()              ← 停止电机硬件
      ├─ 发送 "stopped." 通知
      ├─ free(cmd)                 ← 释放参数内存
      └─ vTaskDelete(NULL)         ← 自删除
```

### 5.3 互斥策略

**同一时刻只有一个定时任务运行** (wifi.c:139-156):

```
新定时指令到达
  └─ if (wifi_timed_task_handle != NULL)
       ├─ vTaskDelete(旧任务)      // 强制终止
       └─ motor_emergency_stop()   // 确保硬件停止
  └─ 创建新任务
```

这意味着 `P 10` 后立即发 `D 15` 会先停止水泵再启动面粉电机.

### 5.4 任务参数

| 参数 | 值 | 说明 |
|------|-----|------|
| 栈大小 | 4096 字节 | ESC 校准有 1s vTaskDelay + 长延时 |
| 优先级 | 5 | 与 TCP 服务器同级 |
| 参数传递 | malloc 堆分配 | 任务结束时 free |

---

## 6. 数据流图

### 6.1 单一指令完整流程

```
  TCP 客户端                     ESP32-S3
     │                              │
     │──── "P 10\r\n" ─────────────→│ recv()
     │                              │
     │                              │ wifi_process_cmd()
     │                              │   ├─ 解析: type='P', val=10
     │                              │   └─ wifi_start_timed_task()
     │                              │       ├─ vTaskDelete(旧任务)
     │                              │       └─ xTaskCreate(新任务)
     │←── "Pump starting..." ──────│ wifi_send_msg()
     │                              │
     │                              │ [FreeRTOS 任务]
     │                              │   ├─ pump_start()
     │                              │   ├─ vTaskDelay(10000ms)
     │                              │   └─ pump_stop()
     │                              │
     │←── "Pump stopped." ─────────│ wifi_send_msg()
     │                              │
```

### 6.2 紧急停止流程

```
  TCP 客户端                     ESP32-S3
     │                              │
     │ 已有 P 10 任务在运行          │ wifi_timed_task_handle ≠ NULL
     │                              │
     │──── "STOP\r\n" ─────────────→│ wifi_process_cmd()
     │                              │   ├─ vTaskDelete(timed_task)
     │                              │   │   └─ 任务被强制终止
     │                              │   └─ motor_emergency_stop()
     │                              │       ├─ g_emergency_stop = true
     │                              │       ├─ step_stop()
     │                              │       ├─ pump_stop()
     │                              │       ├─ grinder_stop()
     │                              │       ├─ mixer_esc_stop()
     │                              │       └─ dough_esc_stop()
     │                              │
     │←── "Emergency Stop All:     │
     │     SUCCESS" ───────────────│
```

---

## 7. 错误处理

### 7.1 Socket 错误

| 错误 | 日志 | 处理 |
|------|------|------|
| `socket()` 失败 | `"Unable to create socket"` | 删除 TCP 任务 |
| `bind()` 失败 | `"Socket unable to bind"` | goto CLEAN_UP |
| `listen()` 失败 | `"Error during listen"` | goto CLEAN_UP |
| `accept()` 失败 | `"Unable to accept connection"` | 退出 while 循环 |
| `recv()` 失败 | `"recv failed: errno %d"` | 断开此客户端 |
| `recv()` 返回 0 | `"Connection closed"` | 断开此客户端 |

### 7.2 指令错误

| 错误 | 响应 |
|------|------|
| 空行 | 静默忽略 |
| 未识别指令 | `"Error: Unknown TCP command"` |
| 解析失败 (无效数字) | `atof`/`atoi` 返回 0, 指令仍执行 |

### 7.3 WiFi 错误

WiFi 初始化使用 `ESP_ERROR_CHECK()` 宏, 任何失败会导致 abort() 重启. 启动后 WiFi 驱动内部处理射频错误 (自动重试).

---

## 8. 配置参考

### 8.1 宏定义速查表

```c
// wifi.h

// WiFi 热点
#define WIFI_SSID      "dough_mixer"   // SSID, 最大 32 字符
#define WIFI_PASS      "12345678"      // 密码, 8~63 字符 (WPA2)
#define WIFI_CHANNEL   1               // 信道 1~13
#define MAX_STA_CONN   4               // 最大 STA 连接数

// TCP 服务器
#define TCP_PORT       8080            // 监听端口
```

### 8.2 修改配置后

修改 `wifi.h` 中的宏定义后需重新编译: `idf.py build flash`

### 8.3 sdkconfig 依赖

WiFi 功能依赖以下 sdkconfig 项 (已启用):

| 项 | 值 |
|----|-----|
| `CONFIG_ESP_WIFI_ENABLED` | y |
| `CONFIG_ESP_WIFI_SOFTAP_SUPPORT` | y |
| `CONFIG_LWIP_MAX_SOCKETS` | ≥ 6 |

---

## 9. 调试指南

### 9.1 串口日志关键点

| 日志 | 含义 | 排查方向 |
|------|------|---------|
| `wifi_init_softap finished` | AP 启动成功 | 手机应能搜到热点 |
| `Socket bound, port 8080` | TCP 端口就绪 | 可尝试连接 |
| `station xx:xx join` | 客户端连上 WiFi | WiFi 层正常 |
| `Socket accepted ip: 192.168.4.x` | TCP 客户端连上 | TCP 层正常 |
| `TCP Received command: [xxx]` | 收到指令 | 检查指令格式 |
| `station xx:xx leave` | 客户端断开 WiFi | 可能距离太远或主动断开 |

### 9.2 常见问题

**手机搜不到 WiFi 热点**
1. 串口确认 `wifi_init_softap finished` 日志
2. 确认 `pmf_cfg.required = false` (已设置)
3. ESP32-S3 的 2.4GHz 天线是否连接

**TCP 连接失败**
1. 确认手机连上了 `dough_mixer` WiFi
2. 确认 IP 是 `192.168.4.1`
3. 确认 main.cpp 中有 `xTaskCreate(tcp_server_task, ...)` 且 `wifi_init_softap()` 先执行
4. 串口是否有 `Socket bound, port 8080` 日志

**TCP 连接后没有响应**
1. 发送的指令是否以回车结尾
2. 发送空行会被忽略, 不会收到任何响应
3. 查看串口日志是否有 `TCP Received command` 确认收到

**指令执行但电机不转**
1. 检查电机接线和 GPIO 引脚
2. 串口日志中 `ledc: GPIO xx is not usable` 表示引脚冲突
3. 用 `STOP` 停止所有后重试

**错误码 104 (ECONNRESET)**
- 客户端断开了连接 (关闭 App、切换网络)
- HTTP 服务器日志中偶尔出现是正常的 (浏览器刷新页面)
