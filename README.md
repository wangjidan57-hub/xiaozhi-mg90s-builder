# XiaoZhi ESP32 + MG90S servo

此仓库用于为 ESP32-S3 的 bread-compact-wifi 小智固件增加 MG90S 舵机语音控制。

## 硬件连接

- MG90S 橙/黄信号线 -> ESP32-S3 GPIO2
- MG90S 红线 -> 稳定 5V 电源正极
- MG90S 棕/黑线 -> GND
- 使用独立 5V 电源时，电源负极必须与 ESP32-S3 GND 共地

不要用 ESP32 的 3.3V 给 MG90S 供电。舵机电流较大，建议独立稳定 5V 电源。

## 语音指令

- “舵机转到 90 度”
- “舵机转到 45 度”
- “舵机转到 180 度”
- “舵机回到中间”

设备端注册 MCP 工具：
- self.servo.set_angle（参数 angle，范围 0..180）
- self.servo.center
- self.servo.get_angle

舵机以 50 Hz PWM 驱动，逻辑角度 0..180 映射到 600..2400 us，采取比常见最大行程更保守的脉宽边界。

## 云端编译

GitHub Actions 工作流会：
1. 检出本仓库的补丁。
2. 获取上游 78/xiaozhi-esp32 的固定源码提交 d7a1c05308fb6e116ffb44b9e2c6e67b3366302a（对应 2.5.3 源码线）。
3. 将补丁覆盖到 bread-compact-wifi 目标板文件。
4. 使用 espressif/idf:v6.1 构建 bread-compact-wifi 中文固件。
5. 生成完整的 merged-binary.bin 并作为 Actions Artifact 提供下载。

构建成功后，到本仓库的 Actions 页面打开最新一次 Build XiaoZhi MG90S firmware 运行，在 Artifacts 中下载 xiaozhi-mg90s-esp32s3-flash-package。压缩包中包含完整合并固件、烧录说明及 SHA256 校验文件。

Flash Download Tool 使用完整合并镜像时，地址设为 0x00000000。烧录前保留原始固件备份，禁止把本文件与旧固件分段混合烧录。
