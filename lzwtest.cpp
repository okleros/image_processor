#include <iostream>
#include <vector>
#include <unordered_map>
#include <cstdint>

// Type aliases for convenience
using Pixel = uint32_t; // Represents an RGB pixel with 8 bits each for R, G, B

// Helper functions to convert between pixel components and Pixel type
inline Pixel toPixel(uint8_t r, uint8_t g, uint8_t b) {
    return (r << 16) | (g << 8) | b;
}

inline uint8_t red(Pixel p) { return (p >> 16) & 0xFF; }
inline uint8_t green(Pixel p) { return (p >> 8) & 0xFF; }
inline uint8_t blue(Pixel p) { return p & 0xFF; }

// Function to compress pixel data using LZW algorithm
std::vector<uint16_t> compressLZW(const std::vector<Pixel>& input) {
    std::unordered_map<std::vector<Pixel>, uint16_t> dictionary;
    std::vector<uint16_t> compressed;
    std::vector<Pixel> w;
    uint16_t dictSize = 256; // Initial dictionary size

    for (Pixel symbol : input) {
        w.push_back(symbol);
        if (dictionary.find(w) == dictionary.end()) {
            if (w.size() > 1) {
                std::vector<Pixel> wPrefix(w.begin(), w.end() - 1);
                compressed.push_back(dictionary[wPrefix]);
            }
            dictionary[w] = dictSize++;
            w.clear();
            w.push_back(symbol);
        }
    }

    if (!w.empty()) {
        compressed.push_back(dictionary[w]);
    }

    return compressed;
}

// Function to decompress LZW compressed data
std::vector<Pixel> decompressLZW(const std::vector<uint16_t>& compressed) {
    std::unordered_map<uint16_t, std::vector<Pixel>> dictionary;
    std::vector<Pixel> decompressed;
    uint16_t dictSize = 256;
    std::vector<Pixel> w;

    for (uint16_t code : compressed) {
        std::vector<Pixel> entry;
        if (code < 256) {
            entry.push_back(static_cast<Pixel>(code));
        } else {
            if (dictionary.find(code) != dictionary.end()) {
                entry = dictionary[code];
            } else {
                entry = w;
                entry.push_back(w[0]);
            }
        }

        decompressed.insert(decompressed.end(), entry.begin(), entry.end());

        if (!w.empty()) {
            std::vector<Pixel> newEntry(w);
            newEntry.push_back(entry[0]);
            dictionary[dictSize++] = newEntry;
        }

        w = entry;
    }

    return decompressed;
}

// Example usage
int main() {
    // Example RGB image data: 2x2 image with 8-bit RGB values
    std::vector<Pixel> imageData = {
        toPixel(255, 0, 0), // Red
        toPixel(0, 255, 0), // Green
        toPixel(0, 0, 255), // Blue
        toPixel(255, 255, 0) // Yellow
    };

    // Compress the image data
    std::vector<uint16_t> compressedData = compressLZW(imageData);
    std::cout << "Compressed data:" << std::endl;
    for (uint16_t code : compressedData) {
        std::cout << code << " ";
    }
    std::cout << std::endl;

    // Decompress the image data
    std::vector<Pixel> decompressedData = decompressLZW(compressedData);
    std::cout << "Decompressed data:" << std::endl;
    for (Pixel pixel : decompressedData) {
        std::cout << "R:" << static_cast<int>(red(pixel)) << " "
                  << "G:" << static_cast<int>(green(pixel)) << " "
                  << "B:" << static_cast<int>(blue(pixel)) << " | ";
    }
    std::cout << std::endl;

    return 0;
}
