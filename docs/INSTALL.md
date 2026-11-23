# PrintPet Core 1.6.1 完整安装／恢复包

本包使用与1.6.0相同的身份区保护布局，修复同名多AP网络可能优先连接弱节点的问题，详见[更新说明](RELEASE_CORE_1.6.1.md)。

适用：FoloToy AI Passport，ESP32-C3、8MiB Flash、无PSRAM。它是社区固件，不是厂商官方固件。

## 先读：不要把应用写到0x0

本包是多文件、分地址的完整包，含配套bootloader、分区表、应用。没有从0x0连续填充的合并镜像，也没有预装任何人的NVS、账号或身份数据。

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
4. 只有已装本次共同分区布局时，才用 `python printpet_flash.py --upgrade --port COM5 --confirm-write`。不匹配会停止，不会偷偷改分区。

脚本先完整备份本机8MiB Flash到private-backups，然后按清单写入并全量回读验证，最后比较NVS/PHY及厂商保留区字节再启动。失败保留日志，不自动重复写或回滚。备份和日志可能含个人资料，**不能发给其他用户或放入GitHub**。不要使用erase_flash、通用合并工具或自己修改偏移。

## 从 Core 1.5.2 / 1.6.0 升级

已安装Core 1.5.2、1.6.0或Spirit 2.0.1共同布局的设备，可使用本包的upgrade模式，仅写Core应用，保留Wi-Fi、云账号、原机身份和其他分区。脚本会先比对真实分区表，不匹配则停止。旧Core 1.5.0/1.5.1、Spirit 2.2.0或原厂/Arduino布局仍需完整install模式，不能直接套用仅应用升级。

升级后首次使用相机：长按OK回菜单，进入配网，展开Camera (optional)，逐机填写打印机IP/主机名和8位LAN访问码；它与云账号Token不同。保存后重启，从进度页按下进入相机页。无需相机的用户可不填写。首次连接按打印机序列号核对并保存证书指纹；只有确认需要重新信任证书时才选择重新配对。普通IP或访问码修改保留原指纹。

未检测到USB、无已知活动任务且连续息屏10分钟会进入深睡，关闭网络监控，需实体按键开机。电脑USB检测不能可靠识别普通充电头/充电宝。相机兼容与供电边界沿用1.6.0，本次修复与实机验证范围见[更新说明](RELEASE_CORE_1.6.1.md)。

## 误刷后的限制

完整程序文件可以用于恢复PrintPet启动，但不能重新生成原机身份、旧Wi-Fi或账号。如果之前把应用写到0x0，NVS、分区和cardid可能早已被覆盖；本包只能保留当前剩余内容。原厂身份只能从**这台机器自己的原始备份**或厂商渠道恢复，不能复制别人的整片Flash。受损NVS启动时可能被固件初始化，需要重新配网。

## 验证边界

本版本的构建、分区/包/脚本检查和实机观察记录随Release提供。此应用已在共享保护布局的ESP32-C3/8MiB设备上升级，连续空闲联网采集32分钟，成功联网后超过31分钟未记录到掉线、心跳超时或崩溃，原20项配置保留。此前同一设备也完成了从Spirit 2.2.0旧布局到Core 1.6.0共享布局的完整迁移。1.6.1完整包的首次安装/恢复路径以离线和模拟Flash检查覆盖，不能把这次应用升级观察扩大为所有安装和负载场景验收。

## 产品区别

Core 1.6.0加入相机预览、页面记忆和电源管理，不显示精灵；Core 1.5.2仍可作为较简洁的1.5.x选择。Spirit 2.0.1提供六位精灵、状态动作和语音，不含本次Core相机更新。这些版本都是只读监控，不提供打印控制；按喜好选择。已处于共同布局后，可通过对应包的upgrade模式切换，配置字段兼容，不清除网络/账号设置。

Spirit 2.0.1沿用此前本地2.2.0六角色功能重新编号，并非退回早期同号实验代码。芙宁娜/胡桃为《原神》/米哈游非官方同人形象；相关声音为AI克隆合成、非商用，原作中文演绎为钱琛/陶典，不暗示官方或演员背书。

## License and source

This package includes LICENSE, LICENSING.md, and the upstream MIT notice in LICENSES. Distributors must provide the corresponding source for MPL-covered files, either with the download or through a source link for the exact build.
