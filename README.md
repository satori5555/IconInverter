
# IconInverter 图标亮度反转工具

## 🧩 简介

**IconInverter** 是一个支持批量处理图标文件的桌面程序，用于反转图标颜色的**亮度（Lightness）**分量，从而达到“反色”或“夜间模式”风格的效果。

程序使用 HSL 色彩模型，在不影响图标色相和饱和度的前提下，仅反转亮度，使得原图在视觉上产生亮/暗翻转的效果。

---

## 🎯 支持格式

| 格式 | 描述                    | 处理方式         |
|------|-------------------------|------------------|
| `.svg`  | 矢量图标（XML）            | 修改 fill/stroke 中的颜色值 |
| `.ico`  | Windows 图标格式（32位）     | 修改原始像素亮度（L 分量） |
| `.jpg` / `.jpeg` | 位图图像               | 使用 OpenCV 处理亮度 |
| `.png`  | 位图图像，含透明通道        | 使用 OpenCV 处理亮度 |
| `.bmp`  | 无压缩图像格式             | 使用 OpenCV 处理亮度 |

---

## 🧪 功能特点

- ✅ 支持批量处理整个文件夹
- ✅ 自动保留原始目录结构
- ✅ 原图不覆盖，保存至新输出目录
- ✅ 支持中文路径
- ✅ 控制台输入或命令行运行皆可

---

## 🛠 使用方法

### 方式一：命令行参数
```bash
IconInverter.exe 输入目录路径 输出目录路径
```

### 方式二：直接双击运行
程序会提示输入源图标目录与输出保存目录：

```
请输入图标输入目录路径: C:/MyIcons
请输入图标输出目录路径: C:/ProcessedIcons
```

---

## 📦 编译依赖

- C++17 标准
- [OpenCV](https://opencv.org/) >= 4.0
- [TinyXML2](https://github.com/leethomason/tinyxml2)

Windows 下可使用 Visual Studio 2022 + CMake 构建：

```bash
mkdir build && cd build
cmake .. -G "Visual Studio 17 2022"
cmake --build . --config Release
```

---

## 📁 项目结构说明

```
├── IconInverter.exe         # 可执行文件
├── main.cpp                 # 主程序代码
├── README.md                # 当前文档
├── tinyxml2.h/.cpp          # XML 解析库（可选）
└── opencv 依赖              # 建议使用 vcpkg 管理
```

---

## 📄 示例效果

原始图标：

![original](./images/sample-before.png)

亮度反转后：

![inverted](./images/sample-after.png)

---

## 🧑‍💻 作者

**古明地 さとり (923957033@qq.com)**

> 如需定制更多图标处理功能（如饱和度调整、色相映射、主题批量替换），欢迎联系作者或提 Issue。

---

## 📃 License

MIT License
