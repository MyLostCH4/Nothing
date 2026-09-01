# 个人数据管理（Qt 6 / C++）

个人长期数据管理软件的桌面端项目。当前版本为 **1.5.3**

## 下载与安装（Windows x64）

 **Releases** 页面：

- [打开最新 Release](https://github.com/MyLostCH4/Nothing/releases/latest)
- [直接下载 Nothing_Setup_1.5.3.exe](https://github.com/MyLostCH4/Nothing/releases/download/v1.5.3/Nothing_Setup_1.5.3.exe)

`Nothing_Setup_1.5.3.exe`。`Source code (zip)` 和 `Source code (tar.gz)` 只是源码压缩包，

### 安装步骤

1. 下载 `Nothing_Setup_1.5.3.exe`。
2. 双击安装包，在询问是否安装 Nothing 时确认继续。
3. 安装程序会部署 Nothing、Qt 6、SQLite 驱动和所需的 VC++ 运行库。
4. 安装完成后 Nothing 自动启动，并生成 `Nothing` 快捷方式。


### 安装位置与升级

- 直接运行更新版本的安装包即可升级，安装过程不会覆盖原有 SQLite 数据。
- 可以在 Windows“设置 → 应用 → 已安装的应用”中卸载 `Nothing`；卸载时可选择保留或删除个人数据。

## 已实现页面

- 极简数据概览
- 收入记录
- 消费记录（简餐、下馆子、下厨、交通、运动补给、零食、医疗支出、购物、日用品、试验垫付）
- 周期消费（按电脑日期自动扣款；首次扣款从开始日期后的下一个周期执行）
- 花呗（总额度 3000 元；支持新增欠款、还款和欠款校准）
- 金库（手动存入和支出，不计入总流动资金）
- 身体记录（体重、羽球、骑行、健身、网球、其他）
- work（理论、试验、小组工作，以及上午/下午/晚上三段工时）
- work 长文本记录可双击打开独立编辑窗口，支持滚动浏览、每秒自动保存和同时打开多条记录
- 支持 1 周至 3 个月范围的运动时长堆叠图与工作时长曲线
- 概览 work 同时显示理论、试验和小组工作的最新进展，三个分区始终等宽

程序已接入 SQLite。新增记录、余额状态、图表录入点和一键清除操作都会立即写入数据库，关闭并重新启动后会自动恢复。全新数据库和清空后的所有数值默认均为 0，不再生成演示数据。

所有记录表格均支持右键选中一行后删除；删除会同步重新计算余额、欠款、概览和图表。

数据库文件位于：`D:\MyQt\PersonalDataManager\data\personal_data.db`

升级到后续版本后，可通过“导入备份”恢复全部记录；新版本会自动迁移旧数据结构。
数据库结构升级前，程序还会在 `data\backups` 自动生成一份 `.nothingdata` 安全备份。

## Windows 安装包

单文件安装包发布在 GitHub Releases，不需要预先安装 Qt 或 Visual Studio。安装包包含 Qt 运行库、Qt SQL、SQLite 驱动和 VC++ 运行库，可直接复制到其他 64 位 Windows 电脑安装。

## 本机编译

项目使用以下本机环境：

- Qt：`D:\MyQt\Qt\6.10.3\msvc2022_64`
- MSVC Build Tools：`D:\MyQt\BuildTools`
- 构建目录：`D:\MyQt\Build\PersonalDataManager`

用 Qt Creator 打开本目录的 `CMakeLists.txt` 即可开发。命令行编译前需先加载 MSVC 的 `vcvars64.bat` 环境。
