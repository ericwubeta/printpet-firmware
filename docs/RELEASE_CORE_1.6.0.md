# PrintPet Core 1.6.0

本版加入可选的局域网相机预览、页面记忆和自动关机，继续保留进度、温度、AMS、HMS/设备信息和多机概览。仍然只读监控，不提供暂停、停止或灯控等打印控制功能，也不包含精灵角色、角色语音、桌面AI聊天或两端联动。

喜欢原有简洁界面的用户可继续使用[Core 1.5.2](https://github.com/ericwubeta/printpet-firmware/releases/tag/v1.5.2)。Spirit 2.0.1是独立的精灵与语音版本，本次只更新Core。

## 更新内容

- **相机第二页**：同时显示机舱JPEG预览、实测FPS、任务、层数、进度、剩余时间和温度。按源图片实际比例完整适配；接收、解码或校验失败时保留上一张有效画面，五秒没有成功更新后显示Loading。离页、换机和息屏会停止旧取图。
- **记住浏览位置**：重启恢复上次打印机与页面；AMS页面等待真实拓扑数据再恢复，旧控制页记录回到同一台打印机的进度页。
- **提示音与连接**：首次历史完成状态不再误播完成音；MQTT客户端标识加入设备MAC和启动随机值，减少不同设备重号造成的连接互踢。
- **息屏与深睡**：真实打印开始/完成或检测到USB插入时请求一次亮屏；电脑USB连接且任一打印机在线打印中时保持亮屏，其余30秒无操作息屏并暂停相机。未检测到USB、没有已知活动任务且连续息屏10分钟后进入深睡；RUNNING、PAUSE、PREPARE任务继续阻止关机。

## 本版修复

- 修改相机IP、访问码、名称、协议或停用/启用时保留原证书指纹。仅明确重新配对才清除指纹；相似字段名、较长参数值和编码NUL不能触发重新配对。配置读取失败或记录不完整时不覆盖信任状态。
- 唤醒期间显示锁忙时，已完成的唤醒手势不会再吞掉下一次独立按键。
- 旧条带显示配置中，未实际绘制即取图失败会保留有效图像和已显示尺寸，后续比例变化仍正确清除黑边；部分绘制后失败继续作废未校验画面。

相机、页面记忆和电源功能来自louisyoungx贡献的[PR #1](https://github.com/ericwubeta/printpet-firmware/pull/1)，本版包含合并前的上述修复与回归测试。

## 下载与升级

下载 **PrintPet-Core-1.6.0-install-recovery.zip**。包内包含配套bootloader、分区表、应用、校验工具和安装说明；不把裸应用作为通用安装包提供。附件同时提供安装说明、验证记录和SHA256。

- 已安装Core 1.5.2或Spirit 2.0.1共同布局：可按包内说明使用upgrade模式，保留Wi-Fi、账号、原机身份和其他分区。
- 旧Core 1.5.0/1.5.1、Spirit 2.2.0或原厂/Arduino布局：使用完整install模式，不能直接套用仅应用升级。工具会比对分区，条件不符时停止。
- 应用地址保持**0x370000**，最大4MiB；只有bootloader放0x0。cardid 0x356000起16KiB只读，整个0x310000–0x36FFFF保留。默认运行工具只离线检查，不访问串口；实际写入需要明确模式、端口和confirm-write。

完整流程见[INSTALL.md](INSTALL.md)。已丢失的原机身份或账号不能从通用固件重建，也不能用别人的整片Flash替代。

相机需在配网页展开Camera (optional)，为对应打印机填写局域网IP/主机名和8位LAN访问码；云Token不能替代LAN访问码。打印机与设备需在局域网可达，并按机型开启LAN Liveview。普通地址或访问码修改保留证书指纹；确认更换设备/证书后才重新配对。

## 兼容与验证边界

- 目标硬件为第三方FoloToy AI Passport：ESP32-C3、8MiB Flash、无PSRAM。社区项目，与FoloToy、Bambu Lab无官方关联。
- 此前旧布局的相机实测仅覆盖X2D，约0.7–1.0 FPS，随图像和网络变化。这是baseline JPEG预览，不解码RTSPS/H.264视频。A1/P1流、其他机型、多机和长期弱网仍待实机验证，不能据协议选项承诺全面兼容。
- 深睡会关闭Wi-Fi/MQTT，此后打印事件不能唤醒设备，需要实体按键开机。USB主机SOF检测不能可靠识别普通充电头/充电宝；电池实际十分钟过程、续航和休眠电流未在本次验证。
- **Build: PASS**：ESP-IDF 5.5.5、LVGL 9.5.0。**Host tests: PASS**：69项相机/协议/状态回归及两套实际MQTT/LVGL只读测试。完整安装包另有离线校验、包负例和模拟Flash保护检查，结果见验证附件。
- **Device tests: NOT RUN**：本次发布二进制与新分区布局未实机刷写；旧版本人工验收不能替代本版实机验收。本次没有连接或刷写设备。发布包使用示例占位配置，不包含开发者账号或设备备份。

历史tag、Release附件和Spirit版本保持原样。

English: Core 1.6.0 adds optional LAN JPEG previews, persistent printer/page selection and idle deep sleep. It remains read-only. Use the complete installation/recovery ZIP; the app belongs at 0x370000, never 0x0. Existing Core 1.5.2/Spirit 2.0.1 shared-layout devices may use guarded upgrade mode. Build and host/package checks passed; this release binary and its layout have not been flashed or physically validated. Earlier camera tests covered X2D only. Deep sleep stops network monitoring and requires a physical-key restart.
