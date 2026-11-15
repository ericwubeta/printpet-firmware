# PrintPet Core 1.6.1

本版修复同名Mesh/扩展器网络可能优先连接弱节点的问题。保留Core 1.6.0的可选局域网相机预览、打印机/页面记忆、息屏唤醒和自动关机，继续只读监控，不提供打印控制功能。

## 更新内容

- **同名网络优先强信号**：先完成全信道扫描，再优先尝试信号更强的同名接入点。此前快速扫描可能遇到第一个同名弱节点就停止；在只有一组已存Wi-Fi的Mesh环境中也可能发生。
- **连接路径统一**：开机、重连沿用的网络配置、手动切网、配网页加入网络和APSTA复用已存网络采用同一策略，不绑定某台路由器MAC。
- **故障信息更清楚**：记录Wi-Fi断开原因码、信号及信道，便于区分无线断链与云端连接异常，不增加密码或Token输出。

密码、认证默认值、MQTT保活、证书验证和消息缓冲配置保持原样；没有通过关闭无线省电或屏蔽错误日志来改变结果。发布应用不含临时诊断任务、SDK Debug日志或故障注入。

## 下载与升级

下载 **PrintPet-Core-1.6.1-install-recovery.zip**。包内含bootloader、分区表、应用、默认离线校验工具、安装和更新说明；附件同时提供安装指南、验证JSON及SHA256SUMS。

- 已安装Core 1.5.2、1.6.0或Spirit 2.0.1共享保护布局：按[INSTALL.md](INSTALL.md)使用upgrade模式，保留Wi-Fi、账号和原机身份。
- 旧Core 1.5.0/1.5.1、Spirit 2.2.0或原厂/Arduino布局：使用完整install模式；分区不匹配时工具会停止，不能套用旧应用地址。
- 应用保持**0x370000**，最大4MiB；只有bootloader放0x0。cardid 0x356000起16KiB只读，整个0x310000–0x36FFFF保留。默认执行工具只离线检查；实际写入需明确模式、端口和confirm-write，并先备份本机数据。

旧版本和Spirit Release继续保留。本次只更新Core。

## 验证结果

- ESP-IDF 5.5.5、LVGL 9.5.0构建通过。71项回归、两套实际MQTT/LVGL只读主机测试、17项包负例和4项内存模拟Flash事务通过。
- 同一位置、同一USB供电、多台同名接入点环境下，修复后的多次重启均选到约−25dBm的强节点；此前曾连接到约−68至−75dBm的弱节点。
- **正式包应用与实机测试二进制逐字节一致**。在一台ESP32-C3/8MiB设备上连续采集1920.3秒（32分钟），保守扣除启动余量后，成功联网的观察时间超过31分钟：已建立云会话断开、Wi-Fi链路掉线、心跳超时和崩溃均为0。首次成功联网前的一次初始连接重试单独统计，不计为已建立会话掉线。
- 运行后读回，原有20项网络/账号/打印机配置全部保留；随后受控重启仍正常联网并触发30秒息屏。

本次实机范围是**USB供电、打印机离线的空闲监控**。应用在既有共享保护布局上升级验证；本完整包的首次安装/恢复路径主要由离线与模拟事务检查覆盖。真实打印数据、相机负载、多机、其他路由器环境和电池十分钟深睡/电流仍待补测。32分钟观察不能作为所有环境下永不掉线的保证。

目标硬件为第三方FoloToy AI Passport（ESP32-C3、8MiB Flash、无PSRAM）。本项目与FoloToy、Bambu Lab无官方关联，发布包不含开发者账号、设备Flash备份或原始串口日志。

English: Core 1.6.1 searches all channels and prefers stronger same-SSID mesh/extender APs, sharing the policy across saved and provisioned connections. The exact released app passed 32 minutes of USB-powered idle observation on one ESP32-C3/8MiB board, with over 31 minutes after cloud connection and no established-session drop, Wi-Fi loss, PING timeout or crash. Live printing, camera load and battery-current validation remain open. Use the complete ZIP; the app belongs at 0x370000, never 0x0.
