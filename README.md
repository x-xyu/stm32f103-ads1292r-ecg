# STM32F103 + ADS1292R ECG System

基于 STM32F103C8T6 与 ADS1292R 的双通道心电采集实验项目，使用 STM32CubeMX 生成 HAL 工程，并通过 Keil MDK-ARM 编译和下载。

## 当前进度

- SPI1 与 ADS1292R 通信正常，芯片 ID 为 `0x73`。
- DRDY 下降沿中断与 250 SPS 连续采样正常。
- USART1 以 115200 bit/s 输出采样数据。
- ADS1292R 内部测试方波已验证，状态字为 `C00000`。
- 下一阶段：蓝牙串口传输，然后在电池供电条件下验证真实 ECG。

## 主要接线

| ADS1292R | STM32F103 |
|---|---|
| MISO | PA6 |
| MOSI | PA7 |
| SCK | PA5 |
| DRDY | PA4 |
| CS0 | PB0 |
| PWDN | PB1 |
| START | PB10 |
| 5V | 5V |
| GND | GND |
| CLK | 不连接 |

## 构建

1. 使用 STM32CubeMX 打开 `test_demo.ioc`。
2. 工具链选择 MDK-ARM。
3. 使用 Keil 打开 `MDK-ARM/test_demo.uvprojx`。
4. 编译并通过 ST-Link 下载。

## 安全

连接人体电极时使用电池供电和无线传输，并断开 ST-Link、USB 转串口、台式电源及接地示波器。本项目仅用于教学实验，不用于医疗诊断。
