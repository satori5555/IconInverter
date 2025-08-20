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
#include <regex>
#include <unordered_map>

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

// 小工具：去空白 & 小写
static inline std::string trim(const std::string& s) {
    size_t b = s.find_first_not_of(" \t\r\n"); if (b == std::string::npos) return "";
    size_t e = s.find_last_not_of(" \t\r\n");  return s.substr(b, e - b + 1);
}
static inline std::string lower(std::string s) { std::transform(s.begin(), s.end(), s.begin(), ::tolower); return s; }

// 解析 #RGB / #RRGGBB / #RRGGBBAA（忽略 alpha）
bool parseHexColor(const std::string& hex, RGB& out) {
    if (hex.empty() || hex[0] != '#') return false;
    std::string s = hex;
    if (s.size() == 4) { // #RGB
        int r = std::stoi(s.substr(1, 1), nullptr, 16);
        int g = std::stoi(s.substr(2, 1), nullptr, 16);
        int b = std::stoi(s.substr(3, 1), nullptr, 16);
        out = RGB{ uint8_t(r * 17), uint8_t(g * 17), uint8_t(b * 17), 255 };
        return true;
    }
    else if (s.size() == 7 || s.size() == 9) { // #RRGGBB / #RRGGBBAA
        int r = std::stoi(s.substr(1, 2), nullptr, 16);
        int g = std::stoi(s.substr(3, 2), nullptr, 16);
        int b = std::stoi(s.substr(5, 2), nullptr, 16);
        out = RGB{ uint8_t(r), uint8_t(g), uint8_t(b), 255 };
        return true;
    }
    return false;
}

// 解析 rgb(...) / rgba(...)，支持空格
bool parseRgbFunc(const std::string& val, RGB& out) {
    std::regex re(R"(rgba?\(\s*([0-9]{1,3})\s*,\s*([0-9]{1,3})\s*,\s*([0-9]{1,3})(?:\s*,\s*([0-9]*\.?[0-9]+|[0-9]{1,3}%))?\s*\))",
        std::regex::icase);
    std::smatch m;
    if (!std::regex_match(val, m, re)) return false;
    int r = std::min(255, std::max(0, std::stoi(m[1].str())));
    int g = std::min(255, std::max(0, std::stoi(m[2].str())));
    int b = std::min(255, std::max(0, std::stoi(m[3].str())));
    out = RGB{ uint8_t(r), uint8_t(g), uint8_t(b), 255 };
    return true;
}

// 少量常见命名色（够用即可；需要更多可自行补充）
bool parseNamedColor(const std::string& val, RGB& out) {
    static const std::unordered_map<std::string, RGB> m = {
        {"black",{0,0,0,255}}, {"white",{255,255,255,255}}, {"red",{255,0,0,255}},
        {"green",{0,128,0,255}}, {"blue",{0,0,255,255}}, {"gray",{128,128,128,255}},
        {"grey",{128,128,128,255}}, {"silver",{192,192,192,255}}, {"maroon",{128,0,0,255}}
    };
    auto it = m.find(lower(trim(val)));
    if (it == m.end()) return false;
    out = it->second; return true;
}

// 统一入口：把颜色字符串解析成 RGB（支持 #hex / rgb(...) / 命名色）
// 遇到 "none"、"transparent"、"currentColor"、"url(#...)" 直接返回 false（不处理）
bool parseColorString(const std::string& raw, RGB& out) {
    std::string s = lower(trim(raw));
    if (s.empty()) return false;
    if (s == "none" || s == "transparent" || s == "currentcolor") return false;
    if (s.rfind("url(", 0) == 0) return false; // 渐变/引用，跳过
    RGB rgb;
    if (parseHexColor(s, rgb)) { out = rgb; return true; }
    if (parseRgbFunc(s, rgb)) { out = rgb; return true; }
    if (parseNamedColor(s, rgb)) { out = rgb; return true; }
    return false;
}

// 反转亮度：输入颜色字符串 -> 返回新的十六进制颜色
bool invertColorString(const std::string& in, std::string& outHex) {
    RGB rgb;
    if (!parseColorString(in, rgb)) return false;
    HSL hsl = rgbToHsl(rgb);
    hsl.l = 1.0f - hsl.l;
    RGB inv = hslToRgb(hsl);
    outHex = rgbToHex(inv);  // 统一输出 #RRGGBB
    return true;
}

// 处理 SVG 文件中的 fill 和 stroke 属性，进行亮度反转
void processSvgFile(const fs::path& input, const fs::path& output) {
    XMLDocument doc;
    if (doc.LoadFile(input.string().c_str()) != XML_SUCCESS) {
        std::cerr << "无法读取: " << input << "\n";
        return;
    }

    // 需要处理的颜色型属性（可自行扩展）
    static const char* kColorAttrs[] = {
        "fill", "stroke", "stop-color", "flood-color", "lighting-color", "color",
        "customFrame" // 你这份 SVG 里出现了这个自定义字段
    };

    auto tryProcessAttr = [&](XMLElement* elem, const char* attrName) {
        const char* val = elem->Attribute(attrName);
        if (!val) return;
        std::string newHex;
        if (invertColorString(val, newHex)) {
            elem->SetAttribute(attrName, newHex.c_str());
        }
        };

    auto processStyleAttr = [&](XMLElement* elem) {
        const char* style = elem->Attribute("style");
        if (!style) return;
        std::string s = style;

        // 简单解析 style="a:b; c:d;"，只改与颜色相关的键
        // 注意：这里不处理复合的 CSS 选择器或变量；够覆盖常见 SVG 图标
        std::string out; out.reserve(s.size() + 16);
        size_t i = 0;
        while (i < s.size()) {
            // 取 key
            size_t keyBeg = i;
            size_t colon = s.find(':', i);
            if (colon == std::string::npos) { out.append(s.substr(i)); break; }
            std::string key = trim(s.substr(keyBeg, colon - keyBeg));
            // 取 value
            size_t semi = s.find(';', colon + 1);
            std::string val = trim(s.substr(colon + 1, (semi == std::string::npos ? s.size() : semi) - (colon + 1)));

            // 是否颜色键
            bool isColorKey = false;
            for (const char* k : kColorAttrs) {
                if (lower(key) == lower(k)) { isColorKey = true; break; }
            }

            if (isColorKey) {
                std::string newHex;
                if (invertColorString(val, newHex)) {
                    val = newHex; // 替换为 #RRGGBB
                }
            }

            // 还原
            out.append(key);
            out.append(": ");
            out.append(val);
            if (semi != std::string::npos) {
                out.push_back(';');
                i = semi + 1;
            }
            else {
                i = s.size();
            }
        }

        elem->SetAttribute("style", out.c_str());
        };

    std::function<void(XMLElement*)> traverse = [&](XMLElement* e) {
        // 1) 直接属性
        for (const char* name : kColorAttrs) {
            tryProcessAttr(e, name);
        }
        // 2) style 属性里的颜色
        processStyleAttr(e);
        // 3) 递归子元素
        for (XMLElement* c = e->FirstChildElement(); c; c = c->NextSiblingElement()) {
            traverse(c);
        }
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

    bool isPngAt(size_t offset) const {
        if (fileData.size() < offset + 8) return false;
        static const uint8_t pngSig[8] = { 0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A };
        return std::memcmp(fileData.data() + offset, pngSig, 8) == 0;
    }

    bool tryRepairIco() {
        // 先尝试 PNG
        std::cout << "[Repair] 尝试自动修复损坏 ICO..." << std::endl;
        // 1. 搜索 PNG
        auto pngIt = std::search(fileData.begin(), fileData.end(),
            "\x89PNG\r\n\x1A\n", "\x89PNG\r\n\x1A\n" + 8);
        if (pngIt != fileData.end()) {
            size_t pngOffset = std::distance(fileData.begin(), pngIt);
            size_t pngLen = fileData.size() - pngOffset;
            cv::Mat tmp = cv::imdecode(std::vector<uint8_t>(fileData.begin() + pngOffset, fileData.end()), cv::IMREAD_UNCHANGED);
            if (!tmp.empty()) {
                // 重组 ICO
                IconDir newHeader{ 0, 1, 1 };
                IconDirEntry newEntry{};
                newEntry.width = static_cast<uint8_t>(tmp.cols);
                newEntry.height = static_cast<uint8_t>(tmp.rows);
                newEntry.colorCount = 0;
                newEntry.reserved = 0;
                newEntry.planes = 1;
                newEntry.bitCount = 32;
                newEntry.bytesInRes = static_cast<uint32_t>(pngLen);
                newEntry.imageOffset = sizeof(IconDir) + sizeof(IconDirEntry);

                fileData.resize(sizeof(IconDir) + sizeof(IconDirEntry) + pngLen);
                std::memcpy(fileData.data(), &newHeader, sizeof(IconDir));
                std::memcpy(fileData.data() + sizeof(IconDir), &newEntry, sizeof(IconDirEntry));
                std::memcpy(fileData.data() + sizeof(IconDir) + sizeof(IconDirEntry),
                    fileData.data() + pngOffset, pngLen);

                header = newHeader;
                entries.clear();
                entries.push_back(newEntry);
                std::cout << "[Repair] ICO 修复成功，已提取单一 32 位 PNG 图标\n";
                return true;
            }
        }
        // 2. 搜索 BMP/DIB 32位图
        for (size_t offset = 0; offset + sizeof(BitmapInfoHeader) < fileData.size(); offset++) {
            const BitmapInfoHeader* bih = reinterpret_cast<const BitmapInfoHeader*>(fileData.data() + offset);
            if (bih->size == 40 && bih->width > 0 && bih->height > 0 &&
                (bih->bitCount == 32 || bih->bitCount == 24) &&
                bih->planes == 1) {
                // 计算像素块大小，ICO 的 DIB 是 height*2，后面有 AND mask
                int width = bih->width;
                int height = bih->height / 2;
                size_t dibSize = sizeof(BitmapInfoHeader) + width * height * (bih->bitCount / 8);
                // 容错：不越界
                if (offset + dibSize > fileData.size()) continue;
                // 可解码，生成新的 ICO
                IconDir newHeader{ 0, 1, 1 };
                IconDirEntry newEntry{};
                newEntry.width = static_cast<uint8_t>(width);
                newEntry.height = static_cast<uint8_t>(height);
                newEntry.colorCount = 0;
                newEntry.reserved = 0;
                newEntry.planes = 1;
                newEntry.bitCount = bih->bitCount;
                newEntry.bytesInRes = static_cast<uint32_t>(dibSize);
                newEntry.imageOffset = sizeof(IconDir) + sizeof(IconDirEntry);

                // 新数据
                std::vector<uint8_t> newFile(sizeof(IconDir) + sizeof(IconDirEntry) + dibSize);
                std::memcpy(newFile.data(), &newHeader, sizeof(IconDir));
                std::memcpy(newFile.data() + sizeof(IconDir), &newEntry, sizeof(IconDirEntry));
                std::memcpy(newFile.data() + sizeof(IconDir) + sizeof(IconDirEntry),
                    fileData.data() + offset, dibSize);

                fileData = std::move(newFile);
                header = newHeader;
                entries.clear();
                entries.push_back(newEntry);
                std::cout << "[Repair] ICO 修复成功，已提取单一 32 位 BMP 图标\n";
                return true;
            }
        }
        std::cerr << "[Repair] 未找到有效 PNG 或 BMP 区块，修复失败\n";
        return false;
    }

public:
    bool loadIco(const std::string& filename) {
        std::ifstream file(filename, std::ios::binary);
        if (!file) return false;
        file.seekg(0, std::ios::end);
        size_t size = file.tellg();
        file.seekg(0);
        fileData.resize(size);
        file.read(reinterpret_cast<char*>(fileData.data()), size);
        if (!file) return false;
        if (size < sizeof(IconDir)) return false;
        std::memcpy(&header, fileData.data(), sizeof(IconDir));
        if (header.count == 0) return false;
        entries.clear();
        size_t entryTableEnd = sizeof(IconDir) + header.count * sizeof(IconDirEntry);
        size_t validCount = 0;
        if (entryTableEnd > size) {
            size_t maxCount = (size > sizeof(IconDir)) ? (size - sizeof(IconDir)) / sizeof(IconDirEntry) : 0;
            for (size_t i = 0; i < maxCount; ++i) {
                IconDirEntry entry;
                std::memcpy(&entry, fileData.data() + sizeof(IconDir) + i * sizeof(IconDirEntry), sizeof(IconDirEntry));
                if (entry.imageOffset + entry.bytesInRes <= size)
                    ++validCount;
                entries.push_back(entry);
            }
        }
        else {
            for (size_t i = 0; i < header.count; ++i) {
                IconDirEntry entry;
                std::memcpy(&entry, fileData.data() + sizeof(IconDir) + i * sizeof(IconDirEntry), sizeof(IconDirEntry));
                if (entry.imageOffset + entry.bytesInRes <= size)
                    ++validCount;
                entries.push_back(entry);
            }
        }
        if (entries.empty() || validCount == 0) {
            std::cerr << "[Warning] ICO 条目无效，尝试修复...\n";
            if (!tryRepairIco()) {
                std::cerr << "[Error] ICO 修复失败，彻底跳过\n";
                return false;
            }
        }
        return true;
    }

	void processHslInversion() {
        bool hasValidImage = false; // 统计至少有1个 entry 能处理
        size_t entryTableEnd = sizeof(IconDir) + entries.size() * sizeof(IconDirEntry);
        for (size_t i = 0; i < entries.size(); ++i) {
            auto& entry = entries[i];
            size_t offset = entry.imageOffset;
            size_t sizeInRes = entry.bytesInRes;
            // 防止 offset 指到文件头、条目表内，或超出文件尾
            if (offset + sizeInRes > fileData.size() || offset < entryTableEnd) {
                std::cerr << "[Warning] 图像数据超出范围，跳过第 " << i << " 个 ICO 图像\n";
                continue;
            }
            hasValidImage = true;

            if (isPngAt(offset)) {
                std::vector<uint8_t> pngData(fileData.begin() + offset, fileData.begin() + offset + sizeInRes);
                cv::Mat img = cv::imdecode(pngData, cv::IMREAD_UNCHANGED);
                if (img.empty()) {
                    std::cerr << "[Warning] PNG 解码失败, 跳过第 " << i << " 个\n";
                    continue;
                }
                if (img.channels() == 4) {
                    for (int y = 0; y < img.rows; ++y) {
                        for (int x = 0; x < img.cols; ++x) {
                            cv::Vec4b& pix = img.at<cv::Vec4b>(y, x);
                            RGB rgb{ pix[2], pix[1], pix[0], pix[3] };
                            HSL hsl = rgbToHsl(rgb);
                            hsl.l = 1.0f - hsl.l;
                            RGB newRgb = hslToRgb(hsl);
                            pix[0] = newRgb.b;
                            pix[1] = newRgb.g;
                            pix[2] = newRgb.r;
                            pix[3] = rgb.a;
                        }
                    }
                }
                else {
                    invertBrightness(img);
                }
                std::vector<uint8_t> outPng;
                cv::imencode(".png", img, outPng);
                if (outPng.size() <= sizeInRes) {
                    std::copy(outPng.begin(), outPng.end(), fileData.begin() + offset);
                    std::fill(fileData.begin() + offset + outPng.size(), fileData.begin() + offset + sizeInRes, 0);
                    entry.bytesInRes = static_cast<uint32_t>(outPng.size());
                }
                else {
                    size_t newOffset = fileData.size();
                    fileData.insert(fileData.end(), outPng.begin(), outPng.end());
                    entry.imageOffset = static_cast<uint32_t>(newOffset);
                    entry.bytesInRes = static_cast<uint32_t>(outPng.size());
                }
            }
            else {
                // BMP 逻辑
                if (sizeInRes < sizeof(BitmapInfoHeader)) {
                    std::cerr << "[Warning] BMP 数据过小，跳过第 " << i << " 个\n";
                    continue;
                }
                BitmapInfoHeader bih;
                std::memcpy(&bih, fileData.data() + offset, sizeof(BitmapInfoHeader));
                if (bih.bitCount != 32) {
                    std::cerr << "[Warning] 非32位BMP，跳过第 " << i << " 个\n";
                    continue;
                }
                int width = bih.width;
                int height = bih.height / 2;
                size_t dataOffset = offset + sizeof(BitmapInfoHeader);
                size_t available = fileData.size() - dataOffset;
                size_t maxPixels = available / 4;
                int safeHeight = std::min(height, static_cast<int>(maxPixels / width));
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
        if (!hasValidImage) {
            std::cerr << "[Info] 该 ICO 没有任何有效图像条目，仅跳过\n";
        }
        // 写回新的 IconDirEntry
        if (!entries.empty()) {
            std::memcpy(fileData.data() + sizeof(IconDir), entries.data(), sizeof(IconDirEntry) * entries.size());
        }
    }

    bool saveIco(const std::string& output) {
        std::ofstream file(output, std::ios::binary);
        if (!file) return false;
        file.write(reinterpret_cast<const char*>(fileData.data()), fileData.size());
        return true;
    }
};

// ------------------- 兜底自动修复 --------------------------

/// OpenCV 兜底强解 ICO -> PNG -> 反色 -> 再自动打包为 ICO（只保留主图层）
bool recoverIcoViaImage(const std::string& inputPath, const std::string& outputPath) {
    // 1. 尝试 OpenCV 强解 ICO
    cv::Mat img = cv::imread(inputPath, cv::IMREAD_UNCHANGED);
    if (img.empty()) {
        // 部分“伪ICO”其实直接是 PNG 数据
        std::ifstream fin(inputPath, std::ios::binary);
        std::vector<uint8_t> buf((std::istreambuf_iterator<char>(fin)), {});
        img = cv::imdecode(buf, cv::IMREAD_UNCHANGED);
    }
    if (img.empty()) return false;

    // 2. 反色处理
    if (img.channels() == 4) {
        for (int y = 0; y < img.rows; ++y)
            for (int x = 0; x < img.cols; ++x) {
                cv::Vec4b& pix = img.at<cv::Vec4b>(y, x);
                RGB rgb{ pix[2], pix[1], pix[0], pix[3] };
                HSL hsl = rgbToHsl(rgb);
                hsl.l = 1.0f - hsl.l;
                RGB out = hslToRgb(hsl);
                pix[0] = out.b; pix[1] = out.g; pix[2] = out.r; // alpha不变
            }
    }
    else {
        invertBrightness(img);
    }

    // 3. 打包为 ICO 格式（PNG嵌入法，通用兼容 Windows 7-11）
    std::vector<uchar> pngBuf;
    if (!cv::imencode(".png", img, pngBuf)) return false;
    // 生成 ICO 结构
    struct IconDir { uint16_t reserved, type, count; };
    struct IconDirEntry {
        uint8_t width, height, colorCount, reserved;
        uint16_t planes, bitCount;
        uint32_t bytesInRes, imageOffset;
    };
    IconDir header{ 0, 1, 1 };
    IconDirEntry entry{};
    entry.width = (uint8_t)img.cols;
    entry.height = (uint8_t)img.rows;
    entry.colorCount = 0;
    entry.reserved = 0;
    entry.planes = 1;
    entry.bitCount = 32;
    entry.bytesInRes = (uint32_t)pngBuf.size();
    entry.imageOffset = sizeof(header) + sizeof(entry);

    std::ofstream out(outputPath, std::ios::binary);
    if (!out) return false;
    out.write((const char*)&header, sizeof(header));
    out.write((const char*)&entry, sizeof(entry));
    out.write((const char*)pngBuf.data(), pngBuf.size());
    return true;
}

// -------------- 文件分派 -----------------
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
                // 兜底恢复
                std::cerr << "[Recover] 尝试 OpenCV 强解 ICO..." << std::endl;
                if (!recoverIcoViaImage(input.string(), output.string())) {
                    std::cerr << "无法加载 ICO: " << input << "\n";
                }
            }
        }
        else if (ext == ".jpg" || ext == ".jpeg" || ext == ".png" || ext == ".bmp") {
            cv::Mat img = cv::imread(input.string(), cv::IMREAD_UNCHANGED);
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