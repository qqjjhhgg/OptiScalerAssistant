# OptiScaler Assistant

> 一键为游戏启用 OptiScaler 帧生成 / 超采样，绕过反作弊注入限制

[![Platform](https://img.shields.io/badge/Platform-Windows%2010%2F11-0078d4)](https://www.microsoft.com/windows)
[![C++17](https://img.shields.io/badge/C%2B%2B-17-00599c)](https://en.cppreference.com/w/cpp/17)
[![WebView2](https://img.shields.io/badge/UI-WebView2-0078d4)](https://developer.microsoft.com/microsoft-edge/webview2)
[![License](https://img.shields.io/badge/License-MIT-green)](LICENSE)

## 功能

- 🎮 **自动识别游戏** - 选择 `game.exe` 自动扫描目录、解析 PE 架构（x64/x86）、识别 RHI 类型（D3D11/D3D12/Vulkan/UE/Unity）
- 🖥️ **GPU 智能检测** - 通过 DXGI 自动检测显卡型号、显存、驱动版本、Tier 分级
- ⚙️ **实时 ini 推荐** - 根据 GPU 等级 + 游戏类型 + 目标分辨率实时生成 OptiScaler 配置（支持 DLSS/FSR3/XeSS）
- 🔌 **注入方式自动选** - D3D12 → `d3d12.dll`；D3D11/UE/Unity → `dxgi.dll`；反作弊严格 → `version.dll`
- 🛡️ **反作弊风险提示** - 内置 30+ 反作弊库扫描，识别 EAC/BattlEye/Xigncode/VAC 等并给出风险等级
- 📦 **单文件部署** - 静态链接 WebView2Loader，DLL 全部嵌入，绿色单 exe
- 🎨 **响应式 UI** - 蓝白配色，4K 屏默认 1235×1680，其他分辨率按比例缩放
- 🪟 **可定制热键** - 内嵌菜单热键（Insert/F1-F12/方向键/Del 等）可在执行前后修改

## 截图

| 主界面 | 风险提示 |
|--------|----------|
| 主窗口显示 GPU / 游戏 / 注入 / ini | 检测到反作弊时弹出 |

## 快速开始

### 用户使用

1. 从 [Releases](../../releases) 下载 `OptiScalerAssistant.exe`（约 194 MB）
2. 双击运行（需要 Windows 10/11 + WebView2 Runtime，已自带或自动下载）
3. 选 `游戏启动程序` → 选 `一键优化` → 启动游戏

### 开发者构建

#### 前置条件

| 工具 | 版本 | 用途 |
|------|------|------|
| MSVC | Visual Studio 2022 Build Tools v143 | 编译 C++17 |
| WebView2 SDK | 1.0+ | 静态库 `WebView2LoaderStatic.lib` |
| Python | 3.12+ | 仅用于生成图标 |
| Pillow | 12+ | `pip install pillow` |

#### 编译

```bat
git clone https://github.com/yourname/OptiScalerAssistant.git
cd OptiScalerAssistant
pack.bat
```

输出：[exe/OptiScalerAssistant.exe](exe/) 单文件

## assets/ 目录

`assets/` 下应放置以下 DLL（**不要提交到 Git 仓库**，单文件超过 100MB 触发 Git LFS）：

| 文件 | 来源 | 大小 |
|------|------|------|
| `OptiScaler.dll` | [OptiScaler releases](https://github.com/optiscaler/OptiScaler/releases) | 25 MB |
| `libxess.dll` | [Intel XeSS SDK](https://github.com/intel/xess) | 78 MB |
| `libxess_dx11.dll` / `libxess_fg.dll` | 同上 | 23 MB |
| `fakenvapi.dll` / `fakenvapi.ini` | OptiScaler 自带 | 0.4 MB |
| `libxell.dll` | OptiScaler 自带 | 0.4 MB |
| `amd_fidelityfx_dx12.dll` | AMD FSR 3.1 SDK | 0.03 MB |
| `amd_fidelityfx_framegeneration_dx12.dll` | 同上 | 38 MB |
| `amd_fidelityfx_upscaler_dx12.dll` | 同上 | 16 MB |
| `amd_fidelityfx_vk.dll` | 同上 | 9 MB |
| `dlssg_to_fsr3_amd_is_better.dll` | OptiScaler 桥 | 3 MB |
| `OptiScaler.ini` | OptiScaler 默认配置 | 0.05 MB |
| `app.ico` | 本项目 | 0.06 MB |

`pack.bat` 会自动嵌入所有上述文件到 exe（通过 `app.rc` 的 `RCDATA` 声明）。Releases 页面下载的 exe 已包含全部资源，开箱即用。

## 项目结构

```
OptiScalerAssistant/
├── src/                        C++ 源代码
│   ├── main.cpp                入口 + 单实例互斥
│   ├── MainWindow.cpp          主窗口、WebView2 桥接、消息路由
│   ├── GpuDetector.cpp         DXGI 显卡检测 + Tier 分级
│   ├── GameScanner.cpp         扫描目录、解析 PE、识别 RHI/反作弊
│   ├── AntiCheatScanner.cpp    反作弊特征库
│   ├── ProfileAdvisor.cpp      C++ 端 ini 生成（备用）
│   ├── Installer.cpp           复制 DLL/INI 到游戏目录、备份、卸载
│   ├── WebView2Host.cpp        WebView2 包装（PostMessage、Resize、DPI）
│   ├── ResourceExtractor.cpp   运行时读取嵌入资源
│   ├── Logger.cpp              日志（文件 + WebView2 终端）
│   └── app.rc / app.manifest   资源、PerMonitorV2 DPI
├── web/
│   └── app.html                全部 UI（HTML/CSS/JS，无外部依赖）
├── assets/                     嵌入资源（运行时释放到游戏目录）
│   ├── OptiScaler.dll          主注入 DLL
│   ├── libxess.dll             Intel XeSS
│   ├── fakenvapi.dll           NVAPI 模拟
│   ├── amd_fidelityfx_*.dll    AMD FSR 3.1
│   ├── dlssg_to_fsr3_*.dll     DLSS-G → FSR3 桥
│   ├── app.ico                 应用图标
│   └── OptiScaler.ini          默认配置
├── exe/                        编译输出
├── pack.bat                    一键编译脚本
├── OptiScalerAssistant.vcxproj 项目文件
├── make_icon.py                从 PNG 生成多帧 ICO
└── README.md
```

## 架构

```
┌─────────────────────────────────────────────────┐
│                WebView2 (HTML/CSS/JS)           │
│   app.html → state / bridge.onMessage / log     │
└──────────────┬──────────────────▲───────────────┘
               │ postMessage       │ PostWebMessageAsString
               ▼                  │
┌──────────────────────────────────────────────────┐
│              C++ Main (Win32)                    │
│   onJsMessage → doScanGame / doDetectGpu        │
│   onWebMessageReceived ← WebView2Host           │
│   GpuDetector / GameScanner / Installer         │
└──────────────────────────────────────────────────┘
```

**通信协议**：JSON over WebView2 `PostWebMessageAsString`

| C++ → JS | 用途 |
|----------|------|
| `gpu-info` | GPU 检测结果 |
| `game-info` | 游戏扫描结果 |
| `screen-resolution` | 屏幕分辨率 |
| `path-pick` | 浏览对话框选中的 exe 路径 |
| `app-ready` | WebView2 初始化完成 |
| `log` | 实时日志推送到前端终端 |

| JS → C++ | 用途 |
|----------|------|
| `ready` | WebView2 ready |
| `detect-gpu` | 重新检测显卡 |
| `browse-folder` | 弹出文件选择对话框 |
| `scan-game` | 扫描游戏目录/exe |
| `optimize` | 执行一键优化 |
| `uninstall` | 卸载 |
| `open-game` | 启动游戏 |

## 推荐配置自动生成

`web/app.html` 里的 `buildRecommendedIni()` 根据以下参数实时生成 ini：

| 维度 | 选项 |
|------|------|
| GPU Vendor | NVIDIA → DLSS / Intel → XeSS / AMD → FSR3 |
| GPU Tier | ultra / high / mid / low |
| 分辨率 | 1920×1080 / 2560×1440 / 3840×2160 / 3440×1440 |
| 游戏 RHI | D3D11 / D3D12 / Vulkan / UE / Unity / Other |
| 反作弊级别 | none / standard / strict / extreme |
| 用户选项 | 超采样、帧生成、锐化、热键 |

输出示例（NVIDIA + D3D12 + 反作弊严格 + 4K）：

```ini
[Upscalers]
Upscaler=dlss
QualityOverride=1
FrameGenMode=2
EnableSharpening=true
Sharpness=0.5

[DLSS]
Preset=K
RenderPresetOverride=true
DlssPreset=K
OverrideMethod=auto

[DllOverrides]
Target=version

[Menu]
ToggleKey=Insert
```

## 技术细节

### DPI 适配

`app.manifest` 声明 `PerMonitorV2`，`WebView2Host::resize()` 直接传物理像素（不再乘 DPI 缩放系数），避免双重缩放导致内容只占一半。

### 4K 窗口尺寸

`MainWindow::create()` 根据屏幕短边动态计算：

```cpp
int shortSide = min(SM_CXSCREEN, SM_CYSCREEN);
if (shortSide >= 2160) { w = 1235; h = 1680; }
else { w = max(800, 1235 * shortSide/2160); h = max(1000, 1680 * shortSide/2160); }
```

### 反作弊风险

- `extreme` (EAC / BattlEye / Xigncode 等) → 弹窗"几乎必然封号"，需用户二次确认
- `strict` (VAC / EasyAntiCheat 老版本) → 弹窗"存在封号风险"
- `standard` / `none` → 不弹窗

### JSON 转义

C++ 端手写 JSON 序列化时 `\` `"` 等字符未转义会导致 `JSON.parse` 失败，路径中 `\` 必须写为 `\\`。已封装 `escapeJsonW()` 处理。

## 许可

MIT License - 详见 [LICENSE](LICENSE)

**注意**：`assets/` 下的 OptiScaler / FSR / XeSS 等 DLL 来自 [OptiScaler 项目](https://github.com/optiscaler/OptiScaler) 及其上游组件，遵循各自的开源许可（GPL / MIT）。发布时已直接嵌入以方便用户使用。

## 致谢

- [OptiScaler](https://github.com/optiscaler/OptiScaler) - 核心代理框架
- AMD FidelityFX SDK - FSR 3.1
- Intel XeSS - XeSS 超采样
- [WebView2](https://developer.microsoft.com/microsoft-edge/webview2) - UI 渲染

## 免责声明

本工具仅用于研究和学习目的。使用 OptiScaler 注入可能违反部分游戏的 EULA，存在封号风险，请自行评估。
