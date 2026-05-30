# 09_lvgl_demo_v8 移植 ESP_demo UI 与联网实现计划书

目标：参考 `11_esp_brookesia_phone` 工程的 LVGL 内存分配方式和 Wi-Fi 联网方法，对 `09_lvgl_demo_v8` 制定实现方案。

本计划书只描述实施步骤，不修改当前工程源码。

## 一、现状分析

### 1. 当前工程 `09_lvgl_demo_v8`

当前主程序位于：

```text
09_lvgl_demo_v8/main/main.c
```

当前显示初始化方式：

```c
bsp_display_cfg_t cfg = {
    .lvgl_port_cfg = ESP_LVGL_PORT_INIT_CONFIG(),
    .buffer_size = BSP_LCD_DRAW_BUFF_SIZE,
    .double_buffer = BSP_LCD_DRAW_BUFF_DOUBLE,
    .flags = {
        .buff_dma = true,
        .buff_spiram = false,
        .sw_rotate = false,
    }
};
```

当前特点：

- LVGL demo 默认运行 `lv_demo_widgets()`。
- 显示 buffer 当前使用 DMA 内存，不是 PSRAM。
- `main/my_ui/` 目录目前为空，可作为 GUI Guider UI 移植目标目录。
- `main/idf_component.yml` 当前只有 `lvgl/lvgl` 依赖。
- `sdkconfig.defaults` 已开启 PSRAM 和 `CONFIG_LV_MEM_CUSTOM=y`，但还没有像参考工程一样显式指定 `LV_MEM_CUSTOM_ALLOC` 使用 PSRAM。

### 2. 参考工程 `11_esp_brookesia_phone`

参考工程中 LVGL 内存策略：

- LVGL 动态内存使用 PSRAM：

```cmake
LV_MEM_CUSTOM_ALLOC(x)=heap_caps_malloc(x, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT)
```

- 显示 buffer 使用整屏双缓冲，并放入 PSRAM：

```c
.buffer_size = BSP_LCD_H_RES * BSP_LCD_V_RES,
.double_buffer = BSP_LCD_DRAW_BUFF_DOUBLE,
.flags = {
    .buff_dma = false,
    .buff_spiram = true,
    .sw_rotate = true,
}
```

参考工程中联网方式：

- 使用 `esp_wifi_remote`。
- ESP32-P4 通过 ESP32-C6 从核/协处理器提供 Wi-Fi。
- 使用标准 ESP-IDF Wi-Fi STA API：

```c
esp_netif_init();
esp_event_loop_create_default();
esp_netif_create_default_wifi_sta();
esp_wifi_init();
esp_wifi_set_mode(WIFI_MODE_STA);
esp_wifi_set_config(WIFI_IF_STA, &wifi_config);
esp_wifi_start();
esp_wifi_connect();
```

### 3. GUI Guider 工程 `ESP_demo`

需要移植的主要目录：

```text
ESP_demo/generated/
ESP_demo/custom/
```

核心 UI 文件：

```text
ESP_demo/generated/gui_guider.c
ESP_demo/generated/gui_guider.h
ESP_demo/generated/setup_scr_home.c
ESP_demo/generated/setup_scr_next.c
ESP_demo/generated/events_init.c
ESP_demo/generated/widgets_init.c
```

图片资源：

```text
ESP_demo/generated/images/_longxia_720x720.c
ESP_demo/generated/images/_xia_alpha_303x264.c
```

字体资源：

```text
ESP_demo/generated/guider_fonts/lv_font_SourceHanSerifSC_Regular_61.c
ESP_demo/generated/guider_fonts/lv_font_SourceHanSerifSC_Regular_51.c
ESP_demo/generated/guider_fonts/lv_font_montserratMedium_18.c
```

GUI Guider UI 特点：

- 屏幕尺寸为 720x720。
- home 页面使用 `_longxia_720x720` 作为背景图。
- home 页面包含时间 label、图片 `_xia_alpha_303x264`、start 按钮。
- start 按钮点击后切换到 next 页面。
- UI 入口函数为：

```c
setup_ui(&guider_ui);
```

## 二、总体实现路线

建议分为两个独立阶段实施：

1. 先完成 LVGL 内存策略调整和 GUI Guider UI 移植，确保屏幕能正常显示。
2. 再加入 Wi-Fi STA 联网功能，连接指定路由器。

这样可以把显示问题和联网问题分开验证，降低调试复杂度。

## 三、LVGL 显示与内存分配改造计划

### 1. 调整 LVGL 动态内存到 PSRAM

计划修改：

```text
09_lvgl_demo_v8/main/CMakeLists.txt
```

增加 LVGL 自定义分配器编译选项：

```cmake
"-DLV_MEM_CUSTOM_ALLOC(x)=heap_caps_malloc(x, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT)"
"-DLV_MEM_CUSTOM_FREE=heap_caps_free"
```

同时确认需要包含：

```c
#include "esp_heap_caps.h"
```

预期效果：

- LVGL 创建 screen、label、button、image object 等对象时，动态内存主要进入 PSRAM。
- 减少内部 SRAM 压力。

注意事项：

- `CONFIG_LV_MEM_CUSTOM=y` 当前已经开启。
- 建议在 `sdkconfig.defaults` 中补充 `CONFIG_SPIRAM_USE_CAPS_ALLOC=y`，与参考工程保持一致。

### 2. 调整显示 buffer 到 PSRAM

计划修改：

```text
09_lvgl_demo_v8/main/main.c
```

将当前显示 buffer 配置由：

```c
.buffer_size = BSP_LCD_DRAW_BUFF_SIZE,
.buff_dma = true,
.buff_spiram = false,
.sw_rotate = false,
```

调整为参考工程方式：

```c
.buffer_size = BSP_LCD_H_RES * BSP_LCD_V_RES,
.double_buffer = BSP_LCD_DRAW_BUFF_DOUBLE,
.flags = {
    .buff_dma = false,
    .buff_spiram = true,
    .sw_rotate = true,
}
```

预期效果：

- LVGL draw buffer 使用整屏双缓冲。
- buffer 分配在 PSRAM。
- 与 GUI Guider 720x720 全屏 UI 更匹配。

内存估算：

如果屏幕为 720x720，RGB565：

```text
单个全屏 buffer = 720 * 720 * 2 = 1,036,800 bytes
双缓冲约 = 2 MB PSRAM
```

### 3. 保留或关闭 LVGL demo

当前 `main.c` 中调用：

```c
lv_demo_widgets();
```

移植 GUI Guider UI 后，应替换为：

```c
setup_ui(&guider_ui);
```

并移除或注释：

```c
lv_demo_widgets();
lv_demo_benchmark();
lv_demo_music();
```

同时建议在 `sdkconfig.defaults` 中关闭不需要的 LVGL demo：

```text
CONFIG_LV_USE_DEMO_WIDGETS=n
CONFIG_LV_USE_DEMO_BENCHMARK=n
CONFIG_LV_USE_DEMO_STRESS=n
CONFIG_LV_USE_DEMO_MUSIC=n
```

这样可以减少固件体积和无关资源占用。

## 四、ESP_demo UI 移植计划

### 1. 建议目标目录结构

建议将 GUI Guider 代码整体放入：

```text
09_lvgl_demo_v8/main/my_ui/
```

推荐结构：

```text
main/my_ui/
  generated/
    gui_guider.c
    gui_guider.h
    setup_scr_home.c
    setup_scr_next.c
    events_init.c
    events_init.h
    widgets_init.c
    widgets_init.h
    images/
      _longxia_720x720.c
      _xia_alpha_303x264.c
    guider_fonts/
      lv_font_SourceHanSerifSC_Regular_61.c
      lv_font_SourceHanSerifSC_Regular_51.c
      lv_font_montserratMedium_18.c
      fonts_list.mk
      guider_fonts.mk
    guider_customer_fonts/
      guider_customer_fonts.h
  custom/
    custom.c
    custom.h
    lv_conf_ext.h
```

也可以简化为：

```text
main/my_ui/generated/
main/my_ui/custom/
```

不建议把 `ESP_demo/lvgl/`、`ESP_demo/lvgl-simulator/`、`ESP_demo/ports/` 复制进 ESP-IDF 工程，因为 `09_lvgl_demo_v8` 已经通过组件依赖使用 LVGL 8.4。

### 2. CMake 纳入 UI 源码

计划修改：

```text
09_lvgl_demo_v8/main/CMakeLists.txt
```

将这些源码加入 `idf_component_register()`：

```text
main.c
my_ui/generated/*.c
my_ui/generated/images/*.c
my_ui/generated/guider_fonts/*.c
my_ui/custom/*.c
```

同时加入 include 目录：

```text
.
my_ui/generated
my_ui/generated/images
my_ui/generated/guider_fonts
my_ui/generated/guider_customer_fonts
my_ui/custom
```

### 3. 处理 GUI Guider 宏定义

GUI Guider 生成代码里有条件编译：

```c
#if LV_USE_GUIDER_SIMULATOR && LV_USE_FREEMASTER
```

ESP-IDF 目标工程中不需要 simulator/freemaster，建议在 CMake 或头文件中统一定义：

```c
LV_USE_GUIDER_SIMULATOR=0
LV_USE_FREEMASTER=0
```

否则可能因为缺少 simulator 相关头文件而编译失败。

### 4. 接入 UI 入口

计划在 `main.c` 中增加：

```c
#include "gui_guider.h"
```

并在 LVGL lock 内调用：

```c
setup_ui(&guider_ui);
```

替换当前：

```c
lv_demo_widgets();
```

建议位置：

```c
bsp_display_lock(0);
setup_ui(&guider_ui);
bsp_display_unlock();
```

### 5. UI 图片资源内存关注点

GUI Guider 图片：

```text
_longxia_720x720
_xia_alpha_303x264
```

其中 `_longxia_720x720` 是整屏 720x720 图片，data_size 为：

```text
518400 * LV_IMG_PX_SIZE_ALPHA_BYTE
```

如果是 RGB565 + alpha，单张图约：

```text
720 * 720 * 3 ≈ 1.48 MB
```

`_xia_alpha_303x264` data_size 为：

```text
79992 * LV_IMG_PX_SIZE_ALPHA_BYTE
```

约：

```text
303 * 264 * 3 ≈ 234 KB
```

这些图片是 `const` 数据，通常进入 flash/rodata，不是 LVGL heap；但 LVGL 渲染、cache、解码过程中可能占用额外 RAM。

建议：

- 先保持 GUI Guider 生成的 C 数组方式，确保移植成功。
- 如果固件体积或 RAM 压力过大，再考虑改为 SPIFFS/SD 加载 PNG/JPEG。
- 保持 `CONFIG_LV_IMG_CACHE_DEF_SIZE=5` 或更低，避免图片 cache 过大。

## 五、联网实现计划

### 1. 联网目标

Wi-Fi 模式：

```text
Station 模式
```

目标 SSID：

```text
Lab107_AX6
```

目标密码：

```text
lab120120.
```

注意：请最终实施前确认密码是否包含末尾英文句点 `.`。如果句点只是中文描述标点，实际密码应改为 `lab120120`。

### 2. 增加组件依赖

计划修改：

```text
09_lvgl_demo_v8/main/idf_component.yml
```

增加：

```yaml
espressif/esp_wifi_remote:
  version: "0.*"
```

同时 `main/CMakeLists.txt` 的组件依赖中需要确保包含：

```text
esp_wifi
esp_event
esp_netif
nvs_flash
```

如果使用 SNTP，再加入：

```text
esp_netif
lwip
```

### 3. sdkconfig 相关配置

参考工程中有：

```text
CONFIG_SLAVE_IDF_TARGET_ESP32C6=y
```

`09_lvgl_demo_v8` 要使用 ESP32-P4 + ESP32-C6 remote Wi-Fi 时，建议同步确认该配置。

建议保留/补充：

```text
CONFIG_SPIRAM=y
CONFIG_SPIRAM_SPEED_200M=y
CONFIG_SPIRAM_XIP_FROM_PSRAM=y
CONFIG_SPIRAM_USE_CAPS_ALLOC=y
CONFIG_SLAVE_IDF_TARGET_ESP32C6=y
```

### 4. 新增 Wi-Fi 模块

建议新增：

```text
main/app_wifi.c
main/app_wifi.h
```

模块职责：

- 初始化 NVS。
- 初始化 `esp_netif`。
- 创建默认事件循环。
- 创建 STA netif。
- 初始化 Wi-Fi。
- 设置 STA 模式。
- 设置 SSID/密码。
- 启动 Wi-Fi。
- 等待连接成功或失败。

推荐 API：

```c
esp_err_t app_wifi_init_sta(void);
bool app_wifi_is_connected(void);
```

### 5. Wi-Fi 事件处理

建议使用 EventGroup：

```text
WIFI_CONNECTED_BIT
WIFI_FAIL_BIT
```

注册事件：

```c
WIFI_EVENT, ESP_EVENT_ANY_ID
IP_EVENT, IP_EVENT_STA_GOT_IP
```

事件策略：

- `WIFI_EVENT_STA_START`：调用 `esp_wifi_connect()`。
- `WIFI_EVENT_STA_DISCONNECTED`：重试连接，达到最大次数后置失败标志。
- `IP_EVENT_STA_GOT_IP`：认为真正联网成功，置 `WIFI_CONNECTED_BIT`。

相比参考工程只看 `WIFI_EVENT_STA_CONNECTED`，使用 `IP_EVENT_STA_GOT_IP` 更可靠，因为连接 AP 不等于已经拿到 IP。

### 6. 凭据保存策略

本次可以先硬编码测试：

```c
#define WIFI_SSID "Lab107_AX6"
#define WIFI_PASS "lab120120."
```

但正式建议：

- 用 Kconfig 配置 SSID/密码。
- 或用 NVS 保存。
- 不在串口日志打印明文密码。

日志建议：

```text
打印 SSID
不打印 password
```

### 7. 与 UI 的关系

第一阶段可以不做联网 UI，只在启动时自动连接 Wi-Fi。

推荐启动顺序：

```text
app_main()
  nvs_flash_init()
  app_wifi_init_sta()
  bsp_display_start_with_config()
  bsp_display_backlight_on()
  setup_ui(&guider_ui)
```

也可以先显示 UI，再异步联网：

```text
app_main()
  nvs_flash_init()
  bsp_display_start_with_config()
  setup_ui(&guider_ui)
  xTaskCreate(wifi_task)
```

推荐使用第二种，避免 Wi-Fi 连接等待阻塞 UI 显示。

## 六、推荐实施步骤

### 阶段 1：准备 UI 文件

1. 将 `ESP_demo/generated/` 复制到 `09_lvgl_demo_v8/main/my_ui/generated/`。
2. 将 `ESP_demo/custom/` 复制到 `09_lvgl_demo_v8/main/my_ui/custom/`。
3. 不复制 `ESP_demo/lvgl/`、`lvgl-simulator/`、`ports/`。
4. 检查 include 路径，确保 `gui_guider.h`、`custom.h`、`events_init.h` 可被找到。

验收：

```text
所有 GUI Guider 源文件都进入 main/my_ui
没有引入 simulator 专用源码
```

### 阶段 2：调整 LVGL 内存方式

1. `main/CMakeLists.txt` 增加 LVGL PSRAM allocator。
2. `main/main.c` 调整显示 buffer 为整屏双缓冲 PSRAM。
3. `sdkconfig.defaults` 补充 PSRAM caps alloc 配置。
4. 关闭不需要的 LVGL demo。

验收：

```text
工程编译通过
启动后不再运行 lv_demo_widgets()
显示 buffer 分配不会占用大量内部 SRAM
```

### 阶段 3：接入 GUI Guider UI

1. `main.c` include `gui_guider.h`。
2. 在 LVGL lock 内调用 `setup_ui(&guider_ui)`。
3. 处理 `LV_USE_GUIDER_SIMULATOR` 和 `LV_USE_FREEMASTER` 宏。
4. 编译并烧录验证 home 页面显示。
5. 点击 start 验证切换 next 页面。

验收：

```text
720x720 背景图正常显示
时间 label 和 start 按钮正常显示
按钮点击能切换 next 页面
无重启、无 LVGL assert
```

### 阶段 4：增加 Wi-Fi 联网

1. `idf_component.yml` 增加 `esp_wifi_remote`。
2. 增加 `app_wifi.c/h`。
3. 初始化 NVS、netif、event loop、Wi-Fi STA。
4. 使用 SSID `Lab107_AX6` 和密码 `lab120120.` 连接。
5. 使用 `IP_EVENT_STA_GOT_IP` 作为联网成功标志。
6. 不打印明文密码。

验收：

```text
串口输出 Wi-Fi start
成功连接 AP
获取 IP 地址
断线后能自动重试
```

### 阶段 5：可选 SNTP 时间同步

如果希望 home 页面时间 label 显示真实时间：

1. 参考 `11_esp_brookesia_phone/components/apps/setting/app_sntp.c`。
2. 新增 `app_sntp.c/h`。
3. Wi-Fi 获取 IP 后初始化 SNTP。
4. 新增一个 UI 更新时间 task，每秒更新 `guider_ui.home_label_1`。
5. 所有 LVGL 对象操作必须包在 `bsp_display_lock()` 和 `bsp_display_unlock()` 内。

验收：

```text
联网后时间从 00:00:00 更新为真实时间
UI 更新无并发崩溃
```

## 七、风险与注意事项

### 1. GUI Guider 图片很大

`_longxia_720x720.c` 源文件约 35 MB，虽然最终只编译当前色深分支，但仍会明显增加编译时间和固件体积。

如固件过大，可考虑：

- 将图片放入 SPIFFS。
- 使用 LVGL 文件系统接口加载图片。
- 将背景图改为 RGB565 无 alpha。
- 压缩为 PNG/JPEG，运行时解码。

### 2. 内部 SRAM 仍需监控

即使 LVGL 大部分放 PSRAM，以下内容仍可能占用内部 SRAM：

- FreeRTOS task stack。
- Wi-Fi/LwIP 内部结构。
- DMA buffer。
- 驱动对象。
- event loop。

建议运行时打印：

```c
heap_caps_get_free_size(MALLOC_CAP_INTERNAL)
heap_caps_get_free_size(MALLOC_CAP_SPIRAM)
heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM)
```

### 3. Wi-Fi 密码是否带句点

用户提供密码为：

```text
lab120120.
```

实施前需确认最后的 `.` 是否为密码的一部分。如果不是，应使用：

```text
lab120120
```

### 4. LVGL 线程安全

Wi-Fi task 或 SNTP task 中不能直接更新 LVGL UI，必须：

```c
bsp_display_lock(0);
// LVGL object operation
bsp_display_unlock();
```

否则可能出现随机崩溃、assert 或显示异常。

## 八、最终建议的文件改动清单

计划实施时预计涉及：

```text
09_lvgl_demo_v8/main/main.c
09_lvgl_demo_v8/main/CMakeLists.txt
09_lvgl_demo_v8/main/idf_component.yml
09_lvgl_demo_v8/sdkconfig.defaults
09_lvgl_demo_v8/main/my_ui/generated/*
09_lvgl_demo_v8/main/my_ui/custom/*
09_lvgl_demo_v8/main/app_wifi.c
09_lvgl_demo_v8/main/app_wifi.h
```

可选：

```text
09_lvgl_demo_v8/main/app_sntp.c
09_lvgl_demo_v8/main/app_sntp.h
```

## 九、推荐完成标准

完成后应满足：

1. `09_lvgl_demo_v8` 不再显示 LVGL widgets demo，而是显示 `ESP_demo` 的 GUI Guider home 页面。
2. LVGL 动态内存和显示 buffer 主要使用 PSRAM。
3. home 页面背景、图片、字体、按钮显示正常。
4. start 按钮可跳转 next 页面。
5. 设备启动后可连接 Wi-Fi `Lab107_AX6`。
6. 串口能看到获取 IP 成功日志。
7. 不在日志中输出明文 Wi-Fi 密码。
8. 如启用 SNTP，home 页面时间可联网后自动更新。
