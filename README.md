# 和面机控制面板 (Dough Mixer)

ESP32-S3 + ST7789V LCD + FT6336U 触摸 + LVGL v9 + EEZ Studio 图形界面

---

## 目录

- [硬件引脚](#硬件引脚)
- [电机参数](#电机参数)
- [LEDC PWM 分配](#ledc-pwm-分配)
- [控制逻辑](#控制逻辑)
- [项目结构](#项目结构)
- [构建与烧录](#构建与烧录)
- [关键配置](#关键配置)
- [WiFi 连接](#wifi-连接)
- [网页控制面板](#网页控制面板)
- [串口 / TCP 指令](#串口--tcp-指令)
- [HTTP API](#http-api)
- [UI 界面](#ui-界面)
- [触摸校准](#触摸校准)
- [常见问题](#常见问题)

---

## 硬件引脚

### SPI LCD — ST7789V (240×320, SPI2)

| 信号 | GPIO | 方向 | 说明 |
|------|------|------|------|
| CS | 1 | OUT | 片选，低电平有效 |
| RST | 2 | OUT | 硬件复位 |
| DC | 42 | OUT | 数据/命令切换 |
| MOSI | 41 | OUT | 主机发从机收 (SDI) |
| SCK | 40 | OUT | SPI 时钟 |
| BL | 39 | OUT | 背光控制，高电平点亮 |

> GPIO 39-42 是 ESP32-S3 JTAG 引脚，如需使用需在 sdkconfig 中禁用 USB Serial/JTAG。

### I2C 触摸 — FT6336U (I2C1)

| 信号 | GPIO | 说明 |
|------|------|------|
| SCL | 38 | I2C 时钟 |
| SDA | 47 | I2C 数据 |
| INT | 21 | 触摸中断 |
| RST | 48 | 触摸复位 |

### 电机

| 电机 | 信号 | GPIO | 驱动方式 |
|------|------|------|---------|
| 步进电机 | STP | 4 | GPTimer 脉冲输出 |
| | DIR | 5 | GPIO 方向控制 |
| | EN | 6 | GPIO 使能 (低有效) |
| 面粉电机 | Dough PWM | 15 | ESC 50Hz PWM |
| 搅拌电机 | Mixer PWM | 9 | ESC 50Hz PWM |
| 水泵 | Pump | 7 | 继电器 |
| 研磨电机 | Grinder | 16 | 继电器 |
| 电磁铁 | Magnet | 18 | 继电器 |
| 转向舵机 | Steering | 8 | 50Hz PWM |
| 推动舵机 | Push | 3 | 50Hz PWM |

---

## 电机参数

### 步进电机

| 参数 | 值 | 说明 |
|------|-----|------|
| 步距角 | 1.8° | 200 步/圈 |
| 微步细分 | 8 | A4988 驱动 |
| 脉冲/圈 | 1600 | 200 × 8 |
| 每脉冲角度 | 0.225° | 360 / 1600 |
| 脉冲频率 | 1 kHz | speed_us=1000 |
| 转速 | 37.5 RPM | 1000 / 1600 × 60 |
| 2.9 圈耗时 | ~4.6 秒 | 推出/收回面粉盆 |

### 面粉电机 (无刷 ESC)

| 参数 | 值 |
|------|-----|
| PWM 频率 | 50Hz |
| 停止/校准 duty | 5% (1ms 脉宽) |
| 运行 duty | 8% (1.6ms 脉宽) |
| 每循环 ON 时间 | 10 秒 |
| 循环间隔 OFF | 5 秒 |
| 出粉速度 | ~5 克/秒 |

### 搅拌电机 (无刷 ESC)

| 参数 | 值 |
|------|-----|
| PWM 频率 | 50Hz |
| 停止/校准 duty | 5% (1ms 脉宽) |
| 运行 duty | 9% (1.8ms 脉宽) |
| 默认搅拌时间 | 1 分钟 |

### 水泵 (继电器)

| 参数 | 值 |
|------|-----|
| 控制方式 | GPIO 继电器 |
| 出水速度 | ~10 克/秒 |
| 水量计算 | weight / 2 (面粉:水=2:1) |

### 舵机

| 参数 | 值 |
|------|-----|
| PWM 频率 | 50Hz |
| 角度范围 | 0° ~ 180° |
| 分辨率 | 14 位 (16384 级) |

### 电磁铁 (继电器)

| 参数 | 值 |
|------|-----|
| GPIO | 18 |
| 控制方式 | GPIO 继电器 |
| 作用 | 和面过程固定面粉盆, 防止移位 |

---

## LEDC PWM 分配

| 定时器 | 通道 | 外设 | GPIO |
|--------|------|------|------|
| LEDC_TIMER_0 | CH0 | 面粉 ESC | 15 |
| LEDC_TIMER_0 | CH1 | 搅拌 ESC | 9 |
| LEDC_TIMER_1 | CH0 | 转向舵机 | 8 |
| LEDC_TIMER_1 | CH1 | 推动舵机 | 3 |

> 定时器 0: 面粉+搅拌 ESC 共用 (50Hz, 14-bit)
> 定时器 1: 舵机专用 (50Hz, 14-bit)

---

## 控制逻辑

### 协同控制流水线

`motor_coordinated_control_ui_http(weight)` 按顺序执行 6 步:

```
步骤1: 电磁铁 ON → 面粉电机循环 (cycles × 10s ON + 5s OFF)
步骤2: 水泵 (根据水量计算时长)
步骤3: 研磨电机 10s
步骤4: 舵机协同动作序列 (~2.5s)
步骤5: 搅拌电机 1 分钟
步骤6: 电磁铁 OFF
```

步进电机的推出/收回通过独立的 `action_step_out` / `action_step_back` 控制 (网页第7/8页)。

每步之间检查 `g_emergency_stop` 标记，置位则提前退出。

### 重量 → 参数转换

```
面粉:水 = 2:1

cycles = (weight + 49) / (DOUGH_G_PER_SEC × 10)
       = (weight + 49) / 50 = 向上取整

pump_time = (weight / 2) / WATER_G_PER_SEC × 1000 (毫秒)
         = weight / 20 (秒)

mixer_minutes = 1 (固定)
```

示例:

| 重量 | 循环次数 | 水泵时长 | 面粉耗时 | 搅拌 |
|------|---------|---------|---------|------|
| 200g | 4 | 10s | ~60s | 1min |
| 500g | 10 | 25s | ~150s | 1min |
| 1000g | 20 | 50s | ~300s | 1min |

### 网页流程

```
第1页 选面食种类 → 第2页 选时间 → 第3页 选重量 → 第4页 选口感
→ 第5页 确认参数 → 点「开始和面」

→ POST /api/start {weight} → 电机运行 (后台)
→ 第6页 运行监控 (计时+状态)

→ 和面完成 → 发酵倒计时 10s
→ 第7页「推出面粉盆」→ POST /api/step_out → CCW 2.9 圈
→ 第8页「收回面粉盆」→ POST /api/step_back → CW 2.9 圈
→ 第9页「恭喜完成」→ 返回首页
```

### EEZ UI 流程

```
EEZ UI (触摸屏):
  选面食(装饰) → 选时间(装饰) → 选重量 → 自定义重量 → 选口感(装饰)
  → 点「开始和面」
  → action_star_mixer() → xTaskCreate(motor_task)
  → motor_coordinated_control_ui_http(weight)
  → 跳转 Mixer 监控页 → Timer 计时页 → Finish 完成页
  → Step Out 推出面粉盆 → action_step_out (CCW 2.9圈)
  → Step Back 收回面粉盆 → action_step_back (CW 2.9圈)
  → End 结束页 → 返回 Main
  → 随时点「紧急停止」→ vTaskDelete(motor_task) + motor_emergency_stop()
```

### 紧急停止机制

1. `g_emergency_stop = true` → 协同控制每步检查后提前返回
2. `vTaskDelete(motor_task_handle)` → 从 UI/HTTP 强制终止电机任务
3. `motor_emergency_stop()` → 逐一停止所有电机硬件

---

## 项目结构

```
dough_mixer/
├── main/
│   ├── main.cpp              # 应用入口, 初始化所有外设
│   ├── CMakeLists.txt
│   └── idf_component.yml     # 组件依赖
├── components/BSP/
│   ├── SPI/                  # SPI2 总线 (LCD 专用)
│   ├── ST7789V_LCD/          # LCD 面板驱动
│   ├── IIC/                  # I2C 总线 (触摸专用)
│   ├── FT6336U_TOUCH/        # 触摸驱动 (兼容 FT5x06)
│   ├── LVGL_UI/              # LVGL 显示+触摸注册
│   ├── MOTOR/                # 电机全控制
│   ├── GPTIM/                # GPTimer 脉冲发生器 (步进)
│   ├── UART/                 # 串口指令
│   ├── WIFI/                 # WiFi AP + TCP 服务器
│   ├── HTTP/                 # HTTP 服务器 + 网页
│   └── ui/                   # EEZ Studio 生成 + 用户编写
│       ├── action.cpp        # Action 实现 (不被 EEZ 覆盖)
│       └── vars.cpp          # 变量实现 (不被 EEZ 覆盖)
├── lv_conf.h                 # LVGL v9 配置
├── partitions.csv            # 分区表 (factory 2MB)
├── sdkconfig                 # ESP-IDF 配置
└── CMakeLists.txt
```

---

## 构建与烧录

### 前置要求

- ESP-IDF v5.5.4
- ESP32-S3 芯片
- 16MB Flash

```bash
idf.py set-target esp32s3
idf.py reconfigure
idf.py build flash monitor
```

---

## 关键配置

| 文件 | 配置项 | 值 |
|------|--------|-----|
| sdkconfig | Flash Size | 16MB |
| sdkconfig | Partition Table | Custom (partitions.csv, factory 2MB) |
| sdkconfig | USB Serial/JTAG | 禁用 (释放 GPIO 39-42) |
| lv_conf.h | Color Depth | 16 (RGB565) |
| lv_conf.h | LV_MEM_SIZE | 64KB |
| lv_conf.h | 字体 | Montserrat 14/18/20 + Simsun 16 CJK + 自定义 5 种 |

---

## WiFi 连接

| 参数 | 值 |
|------|-----|
| SSID | **dough_mixer** |
| 密码 | **12345678** |
| 加密 | WPA2-PSK |
| IP | **192.168.4.1** |
| DHCP 范围 | 192.168.4.2 ~ 192.168.4.254 |

| 端口 | 协议 | 用途 |
|------|------|------|
| 80 | HTTP | 网页控制面板 + REST API |
| 8080 | TCP | 调试命令行 |

---

## 网页控制面板

浏览器打开 `http://192.168.4.1`。

9 页流程: 选种类→选时间→选重量→选口感→确认→监控→推出→收回→完成。

发酵时间在 `http.html` 中配置:

```js
let fermentingDuration = 10;  // 秒

const fermentingDurations = {
    'dumpling': 10,
    'bread': 10,
    'mantou': 10
};
```

---

## 串口 / TCP 指令

串口 (UART0, 115200bps 8N1) 和 TCP (192.168.4.1:8080) 共用指令格式。
**大小写不敏感**，每条以回车结束。

### 全局

| 指令 | 说明 |
|------|------|
| `STOP` | 紧急停止所有电机 |
| `H` | 协同控制 (默认参数) |

### 步进电机 `M`

| 指令 | 说明 | 示例 |
|------|------|------|
| `M A <角度>` | CW 转角度 | `M A 90` |
| `M B <角度>` | CCW 转角度 | `M B 45` |
| `M F <圈数>` | CW 转圈数 | `M F 2.5` |
| `M R <圈数>` | CCW 转圈数 | `M R 1.0` |
| `M START` | 自动往返模式 | `M START` |
| `M STOP` | 停止往返 | `M STOP` |

### 舵机

| 指令 | 说明 | 示例 |
|------|------|------|
| `SS A <角度>` | 转向舵机 0~180° | `SS A 90` |
| `SP A <角度>` | 推动舵机 0~180° | `SP A 45` |

### 定时电机 (创建后台任务, 到时自动停止)

| 指令 | 单位 | 示例 |
|------|------|------|
| `P <秒>` | 水泵 | `P 10` |
| `D <秒>` | 面粉电机 | `D 15` |
| `G <秒>` | 研磨电机 | `G 20` |
| `MI <分钟>` | 搅拌电机 | `MI 5` |

### TCP 示例

```bash
$ nc 192.168.4.1 8080
M F 2.0
Stepper rotated 2.0 turns: SUCCESS
STOP
Emergency Stop All: SUCCESS
```

---

## HTTP API

### `GET /` — 网页

### `POST /api/start` — 启动和面

请求: `{"weight": 500}`
响应: `{"status":"ok","state":"kneading","weight":500}`

### `GET /api/status` — 查询状态

响应: `{"state":"idle"}` — idle | kneading

### `POST /api/stop` — 紧急停止

响应: `{"status":"ok","state":"idle"}`

### `POST /api/step_out` — 推出面粉盆 (CCW 2.9圈)

响应: `{"status":"ok"}`

### `POST /api/step_back` — 收回面粉盆 (CW 2.9圈)

响应: `{"status":"ok"}`

---

## UI 界面

EEZ Studio 构建，11 页:

| 页 | 名称 | ID | 功能 |
|----|------|-----|------|
| 1 | Main | `SCREEN_ID_MAIN` | 选面食种类 (装饰) |
| 2 | Time | `SCREEN_ID_TIME` | 选时间 (装饰) |
| 3 | Weight | `SCREEN_ID_WEIGHT` | 选重量 200~1000g + 自定义 |
| 4 | Weight Set | `SCREEN_ID_WEIGHT_SET` | 自定义重量值输入 |
| 5 | Kougan | `SCREEN_ID_KOUGAN` | 选口感 (装饰) |
| 6 | Mixer | `SCREEN_ID_MIXER` | 运行监控 + 紧急停止 |
| 7 | Timer | `SCREEN_ID_TIMER` | 计时显示页面 |
| 8 | Finish | `SCREEN_ID_FINISH` | 和面完成提示 |
| 9 | Step Out | `SCREEN_ID_STEP_OUT` | 推出面粉盆按钮 |
| 10 | Step Back | `SCREEN_ID_STEP_BACK` | 收回面粉盆按钮 |
| 11 | End | `SCREEN_ID_END` | 完成, 返回首页 |

> 只有重量参数实际传给电机; 种类/时间/口感为装饰性 UI。

### EEZ 变量

| 变量 | 类型 | getter | 说明 |
|------|------|--------|------|
| `dough_weight` | int32 | `get_var_dough_weight` | 面团重量 (克), 实际传给电机 |
| `motor_running` | bool | `get_var_motor_running` | 电机运行状态 |
| `timer_sec` | int32 | `get_var_timer_sec` | 计时器秒数 (空桩) |
| `timer_min` | int32 | `get_var_timer_min` | 计时器分钟 (空桩) |

### EEZ Action 函数

| 函数 | 触发 | 动作 |
|------|------|------|
| `action_dough_sub` | 重量-按钮 | dough_weight -= 50 |
| `action_dough_add` | 重量+按钮 | dough_weight += 50 |
| `action_set_200g` ~ `_1000g` | 预设按钮 | 设固定值 |
| `action_star_mixer` | 开始和面 | 读 weight → 创建 FreeRTOS 任务 → `motor_coordinated_control_ui_http(weight)` |
| `action_stop` | 紧急停止 | `vTaskDelete` 杀电机任务 + `motor_emergency_stop()` |
| `action_step_out` | 推出按钮 | `step_rotate_turns(2.9f, DIR_CCW, 1000)` |
| `action_step_back` | 收回按钮 | `step_rotate_turns(2.9f, DIR_CW, 1000)` |

### EEZ Studio 覆盖规则

`EEZ Build` 会覆盖 `ui/` 下大部分文件。以下用户编写文件**不会被覆盖**:

| 文件 | 内容 |
|------|------|
| `action.cpp` | Action 函数实现 |
| `vars.cpp` | 变量实现 + 初始化 |

`vars.h` 会被覆盖, 如需添加声明需在导出后手动补回。

---

## 触摸校准

若触摸位置和显示不对齐, 调整 `touch.c`:

```c
.flags = {
    .swap_xy = 0,    // 交换 XY
    .mirror_x = 0,   // 水平翻转
    .mirror_y = 0,   // 垂直翻转
},
```

同时确保 `lcd.c` 中的 `mirror`/`swap_xy` 设置与触摸匹配。

---

## 常见问题

| 问题 | 排查方向 |
|------|---------|
| 屏幕白屏 | 检查 SPI 接线, 串口确认 `LCD init done` |
| 屏幕镜像 | 调整 `lcd.c` 中 `esp_lcd_panel_mirror(x,y)` |
| 触摸无响应 | 确认 I2C 接线, 串口应有 `FT6336U 初始化成功` |
| 触摸偏移 | 对齐 `touch.c` 和 `lcd.c` 的 mirror 设置 |
| WiFi 搜不到 | 确认 `pmf_cfg.required = false`, 串口应有 `wifi_init_softap finished` |
| TCP 连不上 | 确认 `main.cpp` 中有 `xTaskCreate(tcp_server_task, ...)` |
| 电机不转 | 串口 `ledc: GPIO xx is not usable` → 引脚冲突, 换空闲 GPIO |
| 紧急停止不生效 | UI 是独立任务, 确认 `action_stop` 中调了 `vTaskDelete` |
| EEZ build 后报错 | `ui.c` 需要 `(void *)` 转换函数指针, `-fpermissive` 已设 |
| 固件超 1MB | 使用 `partitions.csv` (factory 2MB), Flash Size=16MB |
