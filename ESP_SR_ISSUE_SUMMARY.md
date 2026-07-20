# ESP32P4-AIoT 板载麦克风与 ESP-SR 问题总结

> 整理日期：2026-07-20
> 工程：ESP32P4-AIoT
> 文档性质：历史排障记录
> 当前代码状态：工程已回退到仅实现板载麦克风 PCM 采集的版本，ESP-SR、VAD 和 MultiNet 代码已移除。本文保留此前完整问题经过，供后续重新开展语音识别功能时参考。

## 1. 功能背景

项目最初没有完整的板载麦克风语音识别触发链路。为方便分阶段调试，在 AI 页面新增了独立的“语音调试”按键及回调函数，然后逐步尝试实现以下链路：

```text
AI 页面“语音调试”按键
        ↓
启动板载麦克风采集
        ↓
PCM 音频输入
        ↓
AFE / WebRTC VAD 语音活动检测
        ↓
MultiNet7 离线命令词识别
        ↓
串口终端打印识别结果
```

当时选择的是 ESP-SR 离线命令词识别方案，而不是通用连续语音转文字。该方案只能识别预先配置并成功加入 MultiNet 命令图的中文命令，不具备任意语句听写能力。

## 2. 已经确认正常的部分

在接入 ESP-SR 之前，以下基础功能已经实际验证：

- AI 页面原有按键和新增“语音调试”按键布局、点击事件正常。
- “语音调试”独立回调函数能够正常输出调试日志。
- 板载麦克风能够持续读取 PCM 数据。
- 串口能够输出读取次数、累计字节数、峰值和平均绝对值等采样统计。
- ESP-IDF 5.5.4 下，屏幕、UI、Wi-Fi、麦克风等基础功能能够进入应用程序运行阶段。

因此，问题重点并不在 UI 回调或麦克风驱动，而是在 ESP-SR 模型初始化及其运行环境。

## 3. 问题演变过程

### 3.1 ESP-SR 组件下载失败

首次把 ESP-SR 加入组件依赖后，CMake 重新解析依赖时出现：

```text
Cannot establish a connection to the component registry
https://components-file.espressif.com/components/espressif/esp-sr.json
```

原因是构建系统需要访问 Espressif Component Registry 下载 ESP-SR，但当时无法连接网络。这是依赖获取问题，不是代码编译错误。

当时的处理方式：

- 下载并在工程中放置本地 ESP-SR 2.4.5 组件。
- 同时放置 ESP-SR 所需的本地依赖组件。
- 让工程优先使用 `components/` 下的本地组件，避免构建时再次依赖在线下载 ESP-SR。
- 调整模型分区及组件配置，使模型能够随工程烧录。

### 3.2 ESP-SR 初始化触发任务看门狗

ESP-SR 模型开始初始化后，曾出现任务看门狗超时：

```text
task_wdt: Task watchdog got triggered
CPU 1: speech_init
flash_model_info
```

这说明 `speech_init` 长时间停留在模型元数据解析阶段，CPU1 的空闲任务无法及时运行。此时并不是麦克风停止工作，而是模型加载路径发生阻塞。

当时采取的处理方式：

- 把语音模型初始化放到独立后台任务，避免阻塞 UI 主流程。
- 增加分阶段日志，用于区分模型分区映射、AFE 创建、MultiNet 创建和命令配置。
- 改用 ESP-SR 原生 flash-mapped 模型加载方式，移除应用层手工模型搬运方案。
- VAD 改用不需要额外神经网络模型的 WebRTC VAD，避免 `vadnet1_medium` 再次进入有问题的模型解析路径。

### 3.3 MultiNet7 创建阶段崩溃

在 ESP-IDF 5.5.4 下，系统可以正常启动，但点击“语音调试”后，MultiNet7 初始化曾在以下调用链附近崩溃：

```text
memcpy
read_model_params_copy
model_create
MultiNet7 create
```

结合当时日志中的 PSRAM XIP 状态，判断风险集中在：

- ESP-SR 的预编译 MultiNet 库读取模型参数时发生非法内存访问。
- 模型数据位于 Flash 映射区，而程序代码、只读数据或运行时缓冲区又大量依赖 PSRAM。
- ESP32-P4 芯片版本、ESP-IDF、ESP-SR 预编译库、模型格式和 PSRAM 映射方式之间存在兼容性或时序风险。

该问题不是通过增加任务栈或简单延时就能可靠解决。

### 3.4 尝试切换到 ESP-IDF 5.5.2

为了验证是否是 ESP-IDF 5.5.4 与 ESP-SR 2.4.5 的组合问题，曾安装并切换到 ESP-IDF 5.5.2。

切换过程中还处理了以下环境问题：

- VS Code 状态栏仍显示旧版本。
- ESP-IDF Terminal 使用了旧的 `IDF_PATH`。
- VS Code 报缺少 `xtensa-esp-elf` 工具，导致 ESP-IDF 扩展初始化失败。
- 清理并重新配置 VS Code ESP-IDF 环境后，终端能够正确调用指定版本。

但是 ESP-IDF 5.5.2 烧录后出现了更早期、更严重的启动问题：

```text
MSPI Timing: Enter psram timing tuning
ESP-ROM
rst:0x10 (CHIP_LP_WDT_RESET)
```

系统在进入应用程序之前，就在 32 MB AP Memory HEX PSRAM 的时序训练阶段反复被低功耗看门狗复位。把 PSRAM 从 200 MHz 降至 80 MHz 后仍然复现。

这证明当前 ESP32-P4 v1.3 硬件与 ESP-IDF 5.5.2 的 PSRAM 启动支持并不稳定。该问题发生在二级 Bootloader 阶段，与 UI、麦克风或语音识别业务代码无关。

### 3.5 回移看门狗实现没有解决 PSRAM 训练问题

曾把 ESP-IDF 5.5.4 中与 ESP32-P4 WDT 相关的实现回移到 5.5.2，用于排查是否只是 PSRAM 训练期间没有正确喂狗。

新 Bootloader 确实包含了修改，但系统仍然在同一位置复位。这说明：

- WDT 复位只是故障表现。
- 真正原因是 5.5.2 未能完成该硬件的 PSRAM 时序训练。
- 继续修改 5.5.2 的看门狗无法解决底层训练失败。

因此最终撤销了 ESP-IDF 5.5.2 目录中的试验性 WDT 修改，避免污染 SDK。

## 4. 当时最后采用的修复方向

当时不再继续使用无法稳定启动的 ESP-IDF 5.5.2，而是回到已证明能够完成 PSRAM 初始化并进入应用程序的 ESP-IDF 5.5.4，同时尝试采用更保守的运行配置。

### 4.1 固定使用 ESP-IDF 5.5.4

工程 VS Code 配置指向：

```text
D:\esp-idf\.espressif\v5.5.4\esp-idf
```

命令行只读验证结果为：

```text
ESP-IDF v5.5.4
```

### 4.2 降低 PSRAM 频率

当时尝试的配置：

```text
CONFIG_SPIRAM_SPEED_80M=y
CONFIG_SPIRAM_SPEED=80
```

目的：降低 PSRAM 访问时序压力，优先保证系统稳定性。

### 4.3 禁止代码和只读数据从 PSRAM 执行

当时尝试禁用：

```text
# CONFIG_SPIRAM_FETCH_INSTRUCTIONS is not set
# CONFIG_SPIRAM_RODATA is not set
# CONFIG_SPIRAM_XIP_FROM_PSRAM is not set
# CONFIG_SPIRAM_FLASH_LOAD_TO_PSRAM is not set
```

这样应用代码和只读数据继续从 Flash 执行，PSRAM 主要用于堆、音频缓冲区和 ESP-SR 运行时内存，减少 MultiNet 初始化期间复杂的跨存储器访问。

### 4.4 把 ESP-SR 初始化固定到 CPU0

当时把 `speech_init` 后台任务固定到 CPU0：

```c
#define APP_SPEECH_INIT_TASK_CORE 0
```

目的：让 ESP-SR 模型初始化更接近官方示例常见的运行上下文，并避开此前始终发生在 CPU1 上的模型初始化异常。

### 4.5 当时保留的语音识别结构

最后一次 ESP-SR 尝试中曾保留：

- 板载麦克风 PCM 采集模块。
- AI 页面“语音调试”独立按键和回调。
- ESP-SR 2.4.5 本地组件。
- ESP-SR 原生 Flash 模型分区映射。
- AFE 音频前端。
- WebRTC VAD 语音活动检测。
- MultiNet7 中文离线命令词识别。
- 串口输出识别结果。

## 5. ESP-SR 尝试结束时的状态

| 项目 | 当时状态 | 说明 |
|---|---|---|
| UI“语音调试”按键 | 已验证 | 点击和回调日志正常 |
| 板载麦克风 PCM 采集 | 已验证 | 能持续输出采样统计 |
| ESP-SR 本地组件 | 已集成 | 使用本地 ESP-SR 2.4.5 |
| 模型分区映射 | 已实现 | 使用 ESP-SR 原生加载方式 |
| WebRTC VAD | 已实现 | 避免额外 VAD 神经网络模型 |
| MultiNet7 命令识别 | 已实现代码 | 最终配置没有完成稳定实机验证 |
| ESP-IDF 5.5.2 | 已放弃 | 当前硬件卡在 PSRAM 时序训练 |
| ESP-IDF 5.5.4 | 恢复使用 | 已验证能够进入应用程序 |
| 保守稳定性配置 | 已尝试 | 80 MHz PSRAM、禁用 PSRAM XIP、CPU0 初始化 |

## 6. 当时计划的验证方法

由于工程曾在不同 ESP-IDF 版本之间切换，不能继续复用旧的 `build` 缓存。当时计划在新的 ESP-IDF Terminal 中执行：

```powershell
idf.py --version
idf.py fullclean
idf.py -p COM36 build flash monitor
```

第一条命令应显示：

```text
ESP-IDF v5.5.4
```

预期启动日志为：

```text
ESP-IDF v5.5.4
esp_psram: Speed: 80MHz
SPI SRAM memory test OK
```

`MSPI Timing: Enter psram timing tuning` 可以出现一次，但之后应继续启动，不能再次返回 `ESP-ROM` 并循环复位。

点击“语音调试”后，当时预期依次看到类似日志：

```text
Background speech initialization started on CPU0
Speech init stage 1/4: mapping model partition
Speech init stage 2/4: creating AFE/WebRTC VAD runtime
AFE/WebRTC VAD runtime created
Speech init stage 3/4: creating MultiNet runtime
MultiNet runtime created
ESP-SR ready
```

之后对着板载麦克风说已配置的命令词，终端应打印对应的命令 ID、命令文本或识别结果。

## 7. 如果未来重新实现 ESP-SR 后仍然崩溃

如果系统能够正常启动，但仍然在以下位置崩溃：

```text
read_model_params_copy
model_create
MultiNet create
```

则可以基本排除 UI、麦克风采集、任务栈、PSRAM 频率和应用层模型搬运逻辑，问题将集中在 ESP-SR 2.4.5 的预编译 MultiNet7 库、模型文件和当前 ESP32-P4 芯片版本之间的底层兼容性。

这种情况下不应继续用应用层补丁强行绕过，建议选择以下方向之一：

1. 使用 Espressif 或开发板厂商明确验证过的 ESP-IDF、ESP-SR、模型和芯片版本组合。
2. 等待或升级到明确支持当前 ESP32-P4 ECO/芯片版本的 ESP-SR 版本。
3. 保留当前麦克风采集和 VAD，把识别部分切换为云端 ASR，从而获得通用语音转文字能力。

## 8. 历史上涉及的主要工程文件

- `.vscode/settings.json`：VS Code ESP-IDF 环境配置。
- `sdkconfig`、`sdkconfig.defaults`：PSRAM 频率、XIP 和 ESP-SR 配置。
- `partitions.csv`：应用、存储和 ESP-SR 模型分区。
- `main/app_microphone.c`、`main/app_microphone.h`：板载麦克风采集模块。
- `main/app_speech_recognition.c`、`main/app_speech_recognition.h`：当时的 VAD、MultiNet 和识别任务，当前已删除。
- `main/ui/ui.c`、`main/ui/ui_state.c`：AI 页面语音调试按钮与状态处理。
- `main/CMakeLists.txt`：源文件和组件依赖。
- `components/esp-sr/`：当时的本地 ESP-SR 组件，当前已删除。

## 9. 当前回退版本

目前工程仅保留以下链路：

```text
AI 页面“语音调试”按键
        ↓
启动或停止板载麦克风采集
        ↓
读取双通道 16 kHz / 16-bit PCM
        ↓
串口输出读取次数、累计字节数、峰值、平均绝对值和样本值
```

当前版本不会执行语音检测、离线命令识别或通用语音转文字，也不会加载 ESP-SR 模型。

## 10. 结论

本次 ESP-SR 尝试实际遇到了两个不同层级的故障：

1. **ESP-IDF 5.5.2 的 Bootloader 无法在当前硬件上完成 PSRAM 时序训练**，表现为 `CHIP_LP_WDT_RESET` 循环，应用代码尚未开始运行。
2. **ESP-IDF 5.5.4 能够启动，但 MultiNet7 模型创建存在非法内存访问风险**，问题集中在 ESP-SR 预编译库、模型格式、ESP32-P4 芯片版本和存储器映射方式的组合兼容性。

由于基础麦克风采集已经稳定验证，而离线识别链路没有达到可靠运行状态，工程最终回退到仅保留板载麦克风 PCM 采集的版本。本文档保留全部排障经过，作为后续重新选择语音识别方案和版本组合时的参考。
