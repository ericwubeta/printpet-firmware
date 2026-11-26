# PrintPet Spirit 2.0.3 完整安装／恢复包

本版修复重新配网时短信验证码发送失败的问题。选择「配网」后设备会重启进入配网页，暂停后台监控；完成配置或长按 OK 退出后恢复监控。此 ZIP 使用标准引导程序，社区单文件的安装方式见 [社区安装说明](https://github.com/ericwubeta/printpet-firmware/blob/v2.0.3/docs/COMMUNITY-INSTALL.md)。

适用：FoloToy AI Passport，ESP32-C3、8MiB Flash、无PSRAM。它是社区固件，不是厂商官方固件。

## 先读：不要把应用写到0x0

本包是多文件、分地址的完整包，含配套bootloader、分区表、应用和精灵资源。没有从0x0连续填充的合并镜像，也没有预装任何人的NVS、账号或身份数据。

- bootloader：0x0
- 分区表：0x8000
- 精灵资源：0x10000（仅Spirit需要；Core保留资源区域）
- 应用：0x370000，最大4MiB。**不是0x0，也不是旧版的0x10000。**
- 厂商cardid：0x356000起0x4000字节，分区只读；整个0x310000–0x36FFFF保留。

这不是Arduino示例的3MiB应用布局。为避开厂商身份区，PrintPet两条新版本采用共同的专用布局；旧Core1.5.1、Spirit2.2.0或原厂/Arduino布局都不能直接使用新包的“仅应用升级”。首次必须使用完整安装/恢复模式。

## 操作

需要Python3和esptool4.x；可在自己的虚拟环境中运行 `python -m pip install -r requirements.txt`。不要覆盖他人的环境。

1. 解压完整ZIP，保留所有文件。先运行 `python printpet_flash.py`：只核对文件、地址、SHA和分区，不接串口。
2. 关闭占用串口的监视器。确认自己设备的端口。
3. 首次安装／旧布局迁移／误刷恢复：`python printpet_flash.py --install --port COM5 --confirm-write`，把COM5换成实际端口。
4. 只有分区布局和标准引导程序都与本包一致时，才用 `python printpet_flash.py --upgrade --port COM5 --confirm-write`。从社区自解包版、未知引导或旧布局迁移，请使用上一步完整 `--install`；脚本不会仅因分区表相同就放行应用升级。

脚本先完整备份本机8MiB Flash到private-backups，然后按清单写入并全量回读验证，最后比较NVS/PHY及厂商保留区字节再启动。失败保留日志，不自动重复写或回滚。备份和日志可能含个人资料，**不能发给其他用户或放入GitHub**。不要使用erase_flash、通用合并工具或自己修改偏移。

## 误刷后的限制

完整程序文件可以用于恢复PrintPet启动，但不能重新生成原机身份、旧Wi-Fi或账号。如果之前把应用写到0x0，NVS、分区和cardid可能早已被覆盖；本包只能保留当前剩余内容。原厂身份只能从**这台机器自己的原始备份**或厂商渠道恢复，不能复制别人的整片Flash。受损NVS启动时可能被固件初始化，需要重新配网。

## 验证边界

本包中的 2.0.3 应用已在一台 AI Passport 上完成应用区升级、现有 Wi-Fi／账号／打印机配置与原厂身份区回读、重新配网、验证码实际接收和重启恢复监控。完整 ZIP 通过离线校验，包内完整安装脚本路径没有在本轮另行刷写验收。更多记录见 [本版 Release](https://github.com/ericwubeta/printpet-firmware/releases/tag/v2.0.3)。

## 产品区别

Core 1.6.1 提供简洁界面，Spirit 2.0.3 提供六位精灵、状态动作和语音。两条都是只读监控。切换时根据原引导和分区选择上面的安装方式。

Spirit 2.0.1沿用此前本地2.2.0六角色功能重新编号，并非退回早期同号实验代码。芙宁娜/胡桃为《原神》/米哈游非官方同人形象；相关声音为AI克隆合成、非商用，原作中文演绎为钱琛/陶典，不暗示官方或演员背书。

对应源码见 [v2.0.3](https://github.com/ericwubeta/printpet-firmware/tree/v2.0.3)。包内附 `LICENSE`、`LICENSING.md`、`LICENSES/MIT.txt`，媒体及第三方依赖适用各自条款。
