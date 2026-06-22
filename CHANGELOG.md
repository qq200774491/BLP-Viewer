# Changelog

## [1.5.0] - 2026-06-22

### Added

- **Layer composite & border batch operations** — apply image overlays (anchor, margin, scale, opacity) or solid borders (canvas-expand or inset) to all files in the resource list at once
- **Preview toolbar: save with size / alignment** — export the previewed image at a custom resolution with alignment options (stretch, center on transparent background, etc.)
- **Built-in C++ BLP1 codec** — no longer depends on external `blp_lib.dll`; statically compiled, unzip and run
- **Installer improvements** — bilingual (Chinese / English) setup wizard, desktop shortcut option, full uninstall cleanup

### Fixed

- Single-image save now supports format conversion (e.g. BLP → PNG) and correctly overwrites the source file when saving back to the same path
- Fixed decoding failures on some War3 BLP files (CMYK colour space / 3-component / grayscale JPEG payloads)

---

## [1.4.0]

- Batch conversion between BLP / PNG / JPG / BMP / TGA
- Windows Explorer thumbnail integration via `blp_thumbnail.dll` shell extension
- File association manager
- Mipmap level selection in preview
- Alpha channel overlay display

---

# 更新日志

## [1.5.0] - 2026-06-22

### 新功能

- **图层合成 / 边框批量处理** — 对资源列表中的全部图像一键叠加图层（锚点、边距、缩放、透明度）或加边框（扩展画布 / 内描边）
- **预览工具栏：保存尺寸 / 对齐** — 按指定分辨率和对齐方式导出当前预览图
- **编解码器完全内置（C++）** — 不再依赖外部 `blp_lib.dll`，直接解压即用
- **安装包改进** — 双语安装界面、桌面快捷方式选项、卸载时完整清理

### 修复

- 单图保存时支持格式转换（如 BLP → PNG），保存回原路径时正确覆盖原文件
- 修复部分 War3 BLP 文件解码异常（CMYK 色彩空间 / 3 通道 / 灰度 JPEG）

---

## [1.4.0]

- BLP / PNG / JPG / BMP / TGA 批量格式互转
- 通过 `blp_thumbnail.dll` Shell 扩展在资源管理器显示缩略图
- 文件关联管理器
- 预览支持 Mipmap 层级切换
- Alpha 通道叠加显示
