# 和面机控制面板 (Dough Mixer)

ESP32-S3 + ST7789V LCD + FT6336U 触摸 + LVGL v9 + EEZ Studio 图形界面

---

## 目录

- [硬件引脚](#硬件引脚)
- [项目结构](#项目结构)
- [构建与烧录](#构建与烧录)
- [关键配置](#关键配置)
- [WiFi 连接](#wifi-连接)
- [网页控制面板](#网页控制面板)
- [串口指令](#串口指令)
- [TCP 指令](#tcp-指令)
- [HTTP API](#http-api)
- [UI 界面](#ui-界面)
- [电机参数](#电机参数)
- [触摸校准](#触摸校准)
- [常见问题](#常见问题)

---

## 硬件引脚

### SPI LCD (ST7789V, 240×320 竖屏, SPI2)

| 信号 | GPIO | 说明 |
|------|------|------|
| CS | 10 | 片选，低电平有效 |
| RST | 11 | 硬件复位，低电平有效 |
| DC | 12 | 数据/命令切换，高=数据 |
| MOSI (SDI) | 13 | 主机发从机收 |
| SCK | 14 | SPI 时钟 |
| BL | 1 | 背光控制，高电平点亮 |
| MISO | 未连接 | ST7789V 为纯写设备 |

### I2C 触摸 (FT6336U, I2C1)

| 信号 | GPIO | 说明 |
|------|------|------|
| SCL | 21 | I2C 时钟 |
| SDA | 40 | I2C 数据 |
| INT | 41 | 触摸中断 |
| RST | 42 | 触摸复位，低电平复位 |

### 电机

| 电机 | 信号 | GPIO | 类型 |
|------|------|------|------|
| 步进电机 | STP | 4 | 脉冲输出 |
| | DIR | 5 | 方向控制 |
| | EN | 6 | 使能（低有效） |
| 面粉电机 | Dough PWM | 15 | ESC (50Hz PWM) |
| 搅拌电机 | Mixer PWM | 9 | ESC (50Hz PWM) |
| 水泵 | Pump | 7 | 继电器 |
| 研磨电机 | Grinder | 16 | 继电器 |
| 转向舵机 | Steering | 8 | 50Hz PWM |
| 推动舵机 | Push | 3 | 50Hz PWM |

---

## 项目结构

```
dough_mixer/
├── main/
│   ├── main.cpp            # 应用入口，初始化所有外设
│   ├── CMakeLists.txt
│   └── idf_component.yml   # 组件依赖声明
├── components/BSP/
│   ├── CMakeLists.txt
│   ├── SPI/                # SPI 总线 (spi.h/spi.c)
│   ├── ST7789V_LCD/        # LCD 面板驱动 (lcd.h/lcd.c)
│   ├── IIC/                # I2C 总线 (iic.h/iic.c)
│   ├── FT6336U_TOUCH/      # 触摸驱动 (touch.h/touch.c)
│   ├── LVGL_UI/            # LVGL 显示+触摸注册 (lvgl_ui.h/lvgl_ui.c)
│   ├── MOTOR/              # 电机控制 (motor.h/motor.c)
│   ├── GPTIM/              # 通用定时器，步进脉冲 (gptim.h/gptim.c)
│   ├── UART/               # 串口指令解析 (uart.h/uart.c)
│   ├── WIFI/               # WiFi AP + TCP 服务器 (wifi.h/wifi.c)
│   ├── HTTP/               # HTTP 服务器 + 网页 (http.h/http.c/http.html)
│   └── ui/                 # EEZ Studio 生成的 UI 代码
│       ├── ui.h / ui.c         # UI 入口
│       ├── screens.h / screens.c  # 屏幕定义
│       ├── actions.h            # Action 声明 (EEZ 生成)
│       ├── action.cpp           # Action 实现 (用户编写)
│       ├── vars.h               # 变量声明 (EEZ 生成)
│       ├── vars.cpp             # 变量实现 (用户编写)
│       ├── eez-flow.h / eez-flow.cpp  # EEZ Flow 框架
│       └── ui_font_my_character_*.c  # 自定义字体
├── lv_conf.h               # LVGL v9 配置
├── partitions.csv          # 分区表 (factory 2MB)
├── sdkconfig               # ESP-IDF 项目配置
└── CMakeLists.txt          # 根 CMake
```

### 组件依赖 (idf_component.yml)

| 组件 | 版本 | 用途 |
|------|------|------|
| lvgl/lvgl | ^9.4.0 | LVGL 图形库 |
| espressif/esp_lvgl_port | ^2.8.0 | LVGL 到 ESP-IDF 的移植层 |
| espressif/esp_lcd_touch_ft5x06 | ==1.1.0 | FT6336U 触摸驱动 (兼容 FT5x06) |

---

## 构建与烧录

### 前置要求

- ESP-IDF v5.5.4
- ESP32-S3 芯片
- 16MB Flash

### 首次构建

```bash
idf.py set-target esp32s3
idf.py reconfigure
idf.py build
idf.py flash monitor
```

### 增量构建

```bash
idf.py build flash monitor
```

> 修改 `sdkconfig` 后需 `idf.py reconfigure`；EEZ Studio 重新导出 UI 后直接 `idf.py build` 即可。

---

## 关键配置

### sdkconfig

| 配置项 | 值 | 说明 |
|--------|-----|------|
| Flash Size | 16MB | 固件约 1.8MB |
| Partition Table | Custom (partitions.csv) | factory 2MB |
| FreeRTOS | 双核 | LVGL + WiFi 各占一核 |
| PSRAM | 未启用 | — |

### lv_conf.h

| 配置项 | 值 | 说明 |
|--------|-----|------|
| LV_COLOR_DEPTH | 16 | RGB565 |
| LV_MEM_SIZE | 64KB | LVGL 堆大小 |
| LV_DPI_DEF | 130 | 默认 DPI |
| LV_FONT_MONTSERRAT_14 | 1 | 启用 |
| LV_FONT_MONTSERRAT_18 | 1 | 启用 |
| LV_FONT_MONTSERRAT_20 | 1 | 启用 |
| LV_FONT_SIMSUN_16_CJK | 1 | 中文显示 |

---

## WiFi 连接

| 参数 | 值 |
|------|-----|
| SSID | **dough_mixer** |
| 密码 | **12345678** |
| 加密 | WPA2-PSK |
| 信道 | 1 |
| IP | **192.168.4.1** |
| 最大连接数 | 4 |

### TCP 客户端工具

| 平台 | 推荐工具 |
|------|---------|
| Android | TCP Client |
| iOS | TCP Telnet, Mocha Telnet |
| Windows | Putty (Raw TCP), netcat |
| Mac / Linux | `nc 192.168.4.1 8080` |

| 端口 | 服务 | 用途 |
|------|------|------|
| 80 | HTTP | 网页控制面板 |
| 8080 | TCP | 调试命令行 |

---

## 网页控制面板

浏览器打开 `http://192.168.4.1`，选择面食种类→时间→重量→口感→确认→开始。重量选择支持预设 (200/300/500/800/1000g) 和自定义。口感/种类/时间选项为装饰性 UI，不影响硬件执行，只有重量参数真正传递给电机。

---

## 串口指令

串口 UART0（通过 USB 转串口连接），115200bps，8N1。

**大小写不敏感**，每条指令以回车 (\\r 或 \\n) 结束。指令之间会互相阻断——新指令到达时会先停止上一个正在运行的定时任务。

### 全局控制

| 指令 | 说明 | 示例 |
|------|------|------|
| `STOP` | 紧急停止所有电机 | `STOP` |
| `H` | 协同控制（默认参数，面粉5循环+水泵10s+研磨10s+舵机+搅拌1min+步进正反1圈） | `H` |

### 步进电机 `M`

| 指令 | 参数 | 说明 | 示例 |
|------|------|------|------|
| `M A <角度>` | 角度 (度) | 正转指定角度，速度 1000μs/脉冲 | `M A 90` |
| `M B <角度>` | 角度 (度) | 反转指定角度 | `M B 45` |
| `M F <圈数>` | 圈数 | 正转指定圈数 | `M F 2.5` |
| `M R <圈数>` | 圈数 | 反转指定圈数 | `M R 1.0` |
| `M START` | — | 启动自动往返模式 (正180°→停0.5s→反180°) | `M START` |
| `M STOP` | — | 停止自动往返模式 | `M STOP` |

> 步进电机参数: 200步/圈 × 8微步 = 1600脉冲/圈

### 舵机

| 指令 | 参数 | 说明 | 示例 |
|------|------|------|------|
| `SS A <角度>` | 0~180° | 转向舵机 | `SS A 90` |
| `SP A <角度>` | 0~180° | 推动舵机 | `SP A 45` |

### 定时电机

这些指令会创建独立 FreeRTOS 任务，运行指定时间后自动停止。

| 指令 | 参数 | 单位 | 说明 | 示例 |
|------|------|------|------|------|
| `P <秒>` | 秒数 | 秒 | 水泵 | `P 10` |
| `D <秒>` | 秒数 | 秒 | 面粉电机 (ESC 8% duty) | `D 15` |
| `G <秒>` | 秒数 | 秒 | 研磨电机 | `G 20` |
| `MI <分钟>` | 分钟数 | 分钟 | 搅拌电机 (ESC 9% duty) | `MI 5` |

### 串口响应示例

```
> M F 2.0
Stepper rotated 2.0 turns: SUCCESS

> P 10
Pump starting for 10 seconds...
Pump stopped.

> STOP
Emergency Stop All: SUCCESS
```

---

## TCP 指令

TCP 连接到 `192.168.4.1:8080`，指令格式与串口**完全相同**。

响应通过 TCP socket 返回（不在串口监视器中显示），其他行为一致。

```
$ nc 192.168.4.1 8080
M F 2.0
Stepper rotated 2.0 turns: SUCCESS
STOP
Emergency Stop All: SUCCESS
```

---

## HTTP API

### `GET /`

返回网页控制面板 HTML。

### `POST /api/start`

启动电机协同控制（使用传入重量参数）。

**请求:**
```json
{"weight": 500}
```
`weight`: 面团总重量（克），最小值 50

**响应 (成功):**
```json
{"status":"ok","state":"kneading","weight":500}
```

**响应 (忙碌):**
```json
{"status":"busy","state":"kneading"}
```

### `GET /api/status`

查询当前电机状态。

**响应:**
```json
{"state":"idle"}
```

`state` 可能值:
| 值 | 含义 |
|-----|------|
| `idle` | 空闲，可启动 |
| `kneading` | 正在和面 |

### `POST /api/stop`

紧急停止。会销毁电机任务并执行硬件停止。

**响应:**
```json
{"status":"ok","state":"idle"}
```

---

## UI 界面

EEZ Studio 构建的 LVGL 界面，包含 6 个屏幕：

| 页面 | 名称 | 功能 |
|------|------|------|
| 1 | Main | 选择面食种类 (饺子/面包/馒头) |
| 2 | Time | 选择食用时间 (早餐/午餐/晚餐/自定义) |
| 3 | Weight | 选择面团重量 (200~1000g 预设 + 自定义) |
| 4 | Texture | 选择口感偏好 (软糯/适中/有嚼劲) |
| 5 | Confirm | 确认参数汇总，点击"开始和面" |
| 6 | Mixer | 运行监控画面，显示状态+紧急停止按钮 |

> 第 1/2/4 页的选项为装饰性 UI，不影响硬件。只有第 3 页的重量参数 `dough_weight` 实际传给电机。

### 用户编写的文件

以下文件不会被 EEZ Studio 覆盖（不在生成列表中）：

| 文件 | 内容 |
|------|------|
| `ui/action.cpp` | Action 函数实现（加减重量、预设、启动、停止） |
| `ui/vars.cpp` | 全局变量实现（dough_weight 及其 getter/setter） |

### EEZ Studio 重新导出后

`EEZ Studio Build` 会覆盖 `ui/` 下大部分 `.h` 和 `.c` 文件。**不会被覆盖**的文件只有 `action.cpp` 和 `vars.cpp`。

但 `vars.h` 会被覆盖，如果需要在其中添加声明（如 `vars_init()`），每次导出后需要手动补回。

---

## 电机参数

### 步进电机

| 参数 | 值 | 说明 |
|------|-----|------|
| 基础步数/圈 | 200 | 1.8°/步 |
| 微步 | 8 | A4988 驱动 |
| 脉冲/圈 | 1600 | 200×8 |
| 度/脉冲 | 0.225° | 360/1600 |

### 面粉电机 (ESC PWM)

| 参数 | 值 |
|------|-----|
| 频率 | 50Hz |
| 停止/初始化 duty | 5% (1ms) |
| 运行 duty | 8% (1.6ms) |
| 每循环运行时间 | 10s |
| 循环间隔 | 5s |
| 出粉速度 | ~5g/s |

### 搅拌电机 (ESC PWM)

| 参数 | 值 |
|------|-----|
| 频率 | 50Hz |
| 停止/初始化 duty | 5% (1ms) |
| 运行 duty | 9% (1.8ms) |
| 默认搅拌时间 | 2 分钟 |

### 重量→循环转换

```
cycles = weight ÷ (5g/s × 10s) = weight ÷ 50
mixer_minutes = 2 (固定)
```

---

## 触摸校准

触摸坐标通过 `esp_lcd_touch_config_t.flags` 调整：

```c
.flags = {
    .swap_xy = 0,    // 是否交换 XY 轴
    .mirror_x = 0,   // 是否水平翻转
    .mirror_y = 0,   // 是否垂直翻转
},
```

若触摸位置和显示位置对不上，调整上述标志使其与 LCD 的 `mirror`/`swap_xy` 设置匹配。

---

## 常见问题

### 屏幕白屏

1. 检查 SPI 引脚连接 (CS=10, RST=11, DC=12, MOSI=13, SCK=14)
2. 确认背光引脚 `LCD_BL` (GPIO 1) 接对且输出高电平
3. 尝试在 `lcd.c` 中加上 `esp_lcd_panel_invert_color(lcd_panel, true)`
4. 查看串口日志确认 `LCD init done` 和 `LVGL 显示注册成功`

### 屏幕镜像/方向不对

1. `lcd.c` 中调整 `esp_lcd_panel_mirror(lcd_panel, x, y)` 的布尔值
2. `lcd.c` 中调整 `esp_lcd_panel_swap_xy(lcd_panel, true/false)`
3. `lvgl_ui.c` 中调整 `.rotation.swap_xy` / `.rotation.mirror_x` / `.rotation.mirror_y`
4. 同时调整 `touch.c` 中对应的 `mirror_x/mirror_y`

### 触摸不响应 / 位置偏移

1. 检查 I2C 引脚 (SCL=21, SDA=40) 和中断/复位引脚 (INT=41, RST=42)
2. 确认串口有 `FT6336U 触摸初始化成功` 日志
3. 确保 `touch.c` 中的 `mirror_x/mirror_y` 与 LCD 设置匹配

### 紧急停止后电机又启动

旧的电机任务仍在 `vTaskDelay` 中等待。现已改用 `vTaskDelete` 直接销毁电机任务，如果还有问题检查 `action.cpp` 中 `action_stop` 是否调用 `vTaskDelete(g_motor_task_handle)`。

### WiFi 连接不上

1. 串口确认 `wifi_init_softap finished` 日志
2. 确认 `sdkconfig` 中 `CONFIG_USJ_ENABLE_USB_SERIAL_JTAG` 未与 GPIO 39-42 冲突
3. 确认 WiFi SSID 为 `dough_mixer`，密码 `12345678`，加密 WPA2

### 固件太大烧录失败

使用 `partitions.csv`（factory 2MB），确认 `sdkconfig` 中 Flash Size = 16MB。

### TCP 连接失败

确认 `main.cpp` 中有 `xTaskCreate(tcp_server_task, ...)`，且 `wifi_init_softap()` 已先执行。

### GPIO 不可用警告

```
W ledc: GPIO 15/9 is not usable, maybe conflict with others
```

尝试更换面粉/搅拌电机 PWM 引脚到空闲 GPIO（如 47, 48）。

### UART 收到乱码

```
UART: Executing command: [�]
```

UART 引脚 (TX=17, RX=18) 可能有电平漂移或干扰。确保引脚电平稳定，可在 `uart_init` 中配置内部上拉。
