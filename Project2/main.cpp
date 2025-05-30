/*
========================================
【图标亮度反转工具 - IconInverter】
========================================

📌 功能简介:
本程序用于批量处理文件夹内的图标图像文件，对图像进行亮度反转处理（HSL 模型下 L 分量取反），
从而实现对图标的“反色”或“高亮/暗化”效果改造。

✅ 支持格式:
- SVG（直接修改 fill/stroke 中的十六进制颜色）
- ICO（解析像素并处理 32 位真彩色图像）
- JPEG / PNG / BMP（使用 OpenCV 读取并修改像素亮度）

📂 输入输出:
- 输入目录：包含图标图像的文件夹（可递归处理子目录）
- 输出目录：图像处理后的保存目录，保留原始文件相对结构

🛠 处理逻辑：
- 所有图像将按文件扩展名自动识别格式
- 对应格式执行不同的亮度反转逻辑
- 图像处理完后保存到指定输出目录，文件名和结构保持不变

🧭 使用方法:
1. 命令行方式：
    IconInverter.exe <输入文件夹路径> <输出文件夹路径>

2. 无参数启动时将提示用户输入目录路径。

📎 依赖库：
- OpenCV（处理位图图像）
- TinyXML2（解析 SVG）

============================
作者：古明地 さとり (923957033@qq.com)
日期：2025年5月29日
============================
*/

#include <iostream>
#include <fstream>
#include <vector>
#include <cmath>
#include <algorithm>
#include <cstring>
#include <cstdint>
#include <string>
#include <filesystem>
#include <opencv2/opencv.hpp>
#include "tinyxml2.h"

namespace fs = std::filesystem;
using namespace tinyxml2;

// ICO 文件头及图像条目的结构定义
#pragma pack(push, 1)
struct IconDir { uint16_t reserved, type, count; };
struct IconDirEntry {
    uint8_t width, height, colorCount, reserved;
    uint16_t planes, bitCount;
    uint32_t bytesInRes, imageOffset;
};
struct BitmapInfoHeader {
    uint32_t size;
    int32_t width, height;
    uint16_t planes, bitCount;
    uint32_t compression, sizeImage;
    int32_t xPelsPerMeter, yPelsPerMeter;
    uint32_t clrUsed, clrImportant;
};
#pragma pack(pop)

// RGB 与 HSL 的颜色模型定义
struct RGB { uint8_t r, g, b, a; };
struct HSL { float h, s, l; };

// RGB 转 HSL
HSL rgbToHsl(RGB rgb) {
    float r = rgb.r / 255.0f, g = rgb.g / 255.0f, b = rgb.b / 255.0f;
    float max = std::max({ r, g, b }), min = std::min({ r, g, b }), d = max - min;
    HSL hsl; hsl.l = (max + min) / 2.0f;
    if (d == 0) hsl.h = hsl.s = 0;
    else {
        hsl.s = hsl.l > 0.5f ? d / (2 - max - min) : d / (max + min);
        if (max == r) hsl.h = (g - b) / d + (g < b ? 6 : 0);
        else if (max == g) hsl.h = (b - r) / d + 2;
        else hsl.h = (r - g) / d + 4;
        hsl.h /= 6.0f;
    }
    return hsl;
}

// HSL 转 RGB
RGB hslToRgb(HSL hsl) {
    RGB rgb; rgb.a = 255;
    if (hsl.s == 0) {
        // 无饱和度：灰度色
        rgb.r = rgb.g = rgb.b = static_cast<uint8_t>(hsl.l * 255);
    }
    else {
        auto hue2rgb = [](float p, float q, float t) {
            if (t < 0) t += 1; if (t > 1) t -= 1;
            if (t < 1.0f / 6) return p + (q - p) * 6 * t;
            if (t < 0.5f) return q;
            if (t < 2.0f / 3) return p + (q - p) * (2.0f / 3 - t) * 6;
            return p;
            };
        float q = hsl.l < 0.5f ? hsl.l * (1 + hsl.s) : hsl.l + hsl.s - hsl.l * hsl.s;
        float p = 2 * hsl.l - q;
        rgb.r = static_cast<uint8_t>(hue2rgb(p, q, hsl.h + 1.0f / 3) * 255);
        rgb.g = static_cast<uint8_t>(hue2rgb(p, q, hsl.h) * 255);
        rgb.b = static_cast<uint8_t>(hue2rgb(p, q, hsl.h - 1.0f / 3) * 255);
    }
    return rgb;
}

// 处理 OpenCV 图像亮度反转
void invertBrightness(cv::Mat& image) {
    for (int y = 0; y < image.rows; ++y) {
        auto* row = image.ptr<cv::Vec3b>(y);
        for (int x = 0; x < image.cols; ++x) {
            RGB rgb{ row[x][2], row[x][1], row[x][0], 255 };
            HSL hsl = rgbToHsl(rgb);
            hsl.l = 1.0f - hsl.l;
            RGB out = hslToRgb(hsl);
            row[x][0] = out.b;
            row[x][1] = out.g;
            row[x][2] = out.r;
        }
    }
}

// HEX 颜色（如 #AABBCC）转 RGB
RGB hexToRgb(const std::string& hex) {
    if (hex.size() != 7 || hex[0] != '#') return { 0,0,0,255 };
    return {
        static_cast<uint8_t>(std::stoi(hex.substr(1, 2), nullptr, 16)),
        static_cast<uint8_t>(std::stoi(hex.substr(3, 2), nullptr, 16)),
        static_cast<uint8_t>(std::stoi(hex.substr(5, 2), nullptr, 16)),
        255
    };
}

// RGB 转 HEX 颜色字符串
std::string rgbToHex(RGB rgb) {
    char buf[8];
    std::snprintf(buf, sizeof(buf), "#%02X%02X%02X", rgb.r, rgb.g, rgb.b);
    return std::string(buf);
}

// 处理 SVG 文件中的 fill 和 stroke 属性，进行亮度反转
void processSvgFile(const fs::path& input, const fs::path& output) {
    XMLDocument doc;
    if (doc.LoadFile(input.string().c_str()) != XML_SUCCESS) {
        std::cerr << "无法读取: " << input << "\n";
        return;
    }

    auto processAttr = [](XMLElement* elem, const char* attrName) {
        const char* val = elem->Attribute(attrName);
        if (val && val[0] == '#') {
            RGB rgb = hexToRgb(val);
            HSL hsl = rgbToHsl(rgb);
            hsl.l = 1.0f - hsl.l;
            RGB newRgb = hslToRgb(hsl);
            elem->SetAttribute(attrName, rgbToHex(newRgb).c_str());
        }
        };

    std::function<void(XMLElement*)> traverse = [&](XMLElement* e) {
        processAttr(e, "fill");
        processAttr(e, "stroke");
        for (XMLElement* c = e->FirstChildElement(); c; c = c->NextSiblingElement()) traverse(c);
        };
    traverse(doc.RootElement());

    fs::create_directories(output.parent_path());
    doc.SaveFile(output.string().c_str());
}

// ICO 文件处理类，支持亮度反转
class IcoProcessor {
private:
    std::vector<uint8_t> fileData;
    IconDir header;
    std::vector<IconDirEntry> entries;
public:
    bool loadIco(const std::string& filename) {
        std::ifstream file(filename, std::ios::binary);
        if (!file) return false;

        file.seekg(0, std::ios::end);
        std::streampos end = file.tellg();

        // 安全判断：文件是否为空或无法读取
        if (end <= 0) return false;

        size_t size = static_cast<size_t>(end);
        file.seekg(0);

        fileData.resize(size);
        file.read(reinterpret_cast<char*>(fileData.data()), size);

        // 进一步安全判断：实际读取大小是否正确
        if (!file) return false;

        if (size < sizeof(IconDir)) return false;
        std::memcpy(&header, fileData.data(), sizeof(IconDir));

        if (header.count == 0 || size < sizeof(IconDir) + header.count * sizeof(IconDirEntry)) return false;

        entries.resize(header.count);
        std::memcpy(entries.data(), fileData.data() + sizeof(IconDir), sizeof(IconDirEntry) * header.count);
        return true;
    }

    void processHslInversion() {
        for (size_t i = 0; i < entries.size(); ++i) {
            const auto& entry = entries[i];
            size_t offset = entry.imageOffset;

            if (offset + sizeof(BitmapInfoHeader) > fileData.size()) {
                std::cerr << "图像头超出范围，跳过第 " << i << " 个 ICO 图像\n";
                continue;
            }

            BitmapInfoHeader bih;
            std::memcpy(&bih, fileData.data() + offset, sizeof(BitmapInfoHeader));

            if (bih.bitCount != 32) continue;

            int width = bih.width;
            int height = bih.height / 2;
            size_t dataOffset = offset + sizeof(BitmapInfoHeader);
            size_t available = fileData.size() - dataOffset;

            size_t maxPixels = available / 4;
            int safeHeight = std::min(height, static_cast<int>(maxPixels / width));

            if (safeHeight < height) {
                std::cerr << "像素数据不足，图像高度裁剪为 " << safeHeight << "\n";
            }

            for (int y = 0; y < safeHeight; ++y) {
                for (int x = 0; x < width; ++x) {
                    size_t pix = dataOffset + ((safeHeight - 1 - y) * width + x) * 4;
                    if (pix + 3 >= fileData.size()) continue;

                    RGB rgb{ fileData[pix + 2], fileData[pix + 1], fileData[pix + 0], fileData[pix + 3] };
                    HSL hsl = rgbToHsl(rgb);
                    hsl.l = 1.0f - hsl.l;
                    RGB newRgb = hslToRgb(hsl);
                    fileData[pix + 0] = newRgb.b;
                    fileData[pix + 1] = newRgb.g;
                    fileData[pix + 2] = newRgb.r;
                }
            }
        }
    }

    bool saveIco(const std::string& output) {
        std::ofstream file(output, std::ios::binary);
        if (!file) return false;
        file.write(reinterpret_cast<const char*>(fileData.data()), fileData.size());
        return true;
    }
};

// 根据文件类型进行单个文件处理
void processFile(const fs::path& input, const fs::path& output) {
    std::string ext = input.extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
    fs::create_directories(output.parent_path());

    try {
        if (ext == ".svg") {
            processSvgFile(input, output);
        }
        else if (ext == ".ico") {
            IcoProcessor proc;
            if (proc.loadIco(input.string())) {
                proc.processHslInversion();
                proc.saveIco(output.string());
            }
            else {
                std::cerr << "无法加载 ICO: " << input << "\n";
            }
        }
        else if (ext == ".jpg" || ext == ".jpeg" || ext == ".png" || ext == ".bmp") {
            cv::Mat img = cv::imread(input.string());
            if (!img.empty()) {
                invertBrightness(img);
                cv::imwrite(output.string(), img);
            }
            else {
                std::cerr << "无法读取图像: " << input << "\n";
            }
        }
        else {
            std::cerr << "不支持的文件格式: " << input << "\n";
        }
    }
    catch (const std::exception& e) {
        std::cerr << "处理失败: " << input << "\n原因: " << e.what() << "\n";
    }
}

// 遍历目录，批量处理所有图标
void batchProcess(const std::string& inputDir, const std::string& outputDir) {
    for (const auto& entry : fs::recursive_directory_iterator(inputDir)) {
        if (!entry.is_regular_file()) continue;

        fs::path relative = fs::relative(entry.path(), inputDir);
        fs::path outPath = fs::path(outputDir) / relative;

        processFile(entry.path(), outPath);
        std::cout << "已处理: " << entry.path() << "\n";
    }
}

int main(int argc, char* argv[]) {
    std::string inDir, outDir;

    if (argc >= 3) {
        inDir = argv[1];
        outDir = argv[2];
    }
    else {
        std::cout << "请输入图标输入目录路径: ";
        std::getline(std::cin, inDir);

        std::cout << "请输入图标输出目录路径: ";
        std::getline(std::cin, outDir);
    }

    std::cout << "图标亮度反转工具启动\n输入目录: " << inDir << "\n输出目录: " << outDir << "\n\n";
    batchProcess(inDir, outDir);
    std::cout << "\n全部处理完成！\n";
    return 0;
}
