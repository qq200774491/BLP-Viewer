# BLP Viewer / 图像快速处理工具

Windows 下的 BLP（暴雪纹理格式，War3）查看与批量转换工具：
Dear ImGui + DirectX11 前端，支持 BLP / PNG / JPG / BMP / TGA 预览、Alpha 显示、
拖拽批量互转，并附带资源管理器缩略图 shell 扩展。

BLP 编解码为内置 C++ 实现（`gui/src/blp/blp_codec.*`）：

- **解码**：BLP1 调色板（1/4/8-bit alpha）与 JPEG-content 两种变体，带 mip 级回退容错
- **编码**：JPEG-content BLP1（libjpeg-turbo），完整 mipmap 链，quality 0–100
- 不支持 BLP2（WoW）

## 构建

需要 Visual Studio（x64）与 CMake：

```powershell
.\gui\build.ps1 -Config Release
```

产物在 `gui/build/Release/`：`blp_viewer.exe`、`blp_thumbnail.dll`（缩略图 shell 扩展）。
依赖均已 vendored（`gui/third_party/`：imgui、stb、turbojpeg 预编译静态库），无需额外安装。

## 测试

```powershell
.\gui\build\Release\blp_codec_selftest.exe test-data\blp test-data\png
```

对 `test-data/` 下的样本做解码回归（与参考 PNG 比 PSNR）和编码往返验证。

## 安装包

需要 Inno Setup 6：

```powershell
.\gui\installer\build_installer.ps1 -Config Release
```

输出 `gui/installer/dist/BLP_Viewer_Setup_x64.exe`。

## 历史

本项目早期通过 Rust `blp` crate（blp_lib.dll）提供编解码，已在 2026-06 替换为内置
C++ 实现以缩减体积与依赖；Rust 实现可在 git 历史中找到。
