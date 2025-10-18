
#include "../include/Particle.h"
#include "../include/Parse.h"
#include "../include/Grid.h"
#include <filesystem>
#include <windows.h>
#include <iostream>  


namespace parse {

size_t getSizePCD(const char* file) {
    FILE* fp = fopen(file, "r");
    if (!fp) return 0;

    size_t count = 0;
    const size_t BUF_SIZE = 1 << 20; // 1 MB
    char* buf = new char[BUF_SIZE];
    while (size_t n = fread(buf, 1, sizeof(buf), fp)) {
        for (size_t i = 0; i < n; ++i)
            if (buf[i] == '\n') ++count;
    }
    fclose(fp);
    return count;
}



float parseFloat4Decimal(char*& data) {
    while (*data == ' ') ++data;

    int sign = 1;
    if (*data == '-') { sign = -1; ++data; }

    int intPart = 0;
    while (*data >= '0' && *data <= '9') {
        intPart = intPart * 10 + (*data - '0');
        ++data;
    }

    ++data; // skip decimal point

    int fracPart = 0;
    while (*data >= '0' && *data <= '9') {
        fracPart = fracPart * 10 + (*data - '0');
        ++data;
    }

    while (*data == '\n' || *data == '\r' || *data == ' ') ++data;

    float value = sign * (intPart + fracPart * 0.0001f);

    return value;
}


void readXYZFast(const char* file, std::vector<particle::Particle>& particles) {

    size_t particleCount = getSizePCD(file);
    particles.reserve(particleCount);


    HANDLE hFile = CreateFileA(
        file, GENERIC_READ, FILE_SHARE_READ, NULL,
        OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);

    if (hFile == INVALID_HANDLE_VALUE) {
        std::cerr << "Failed to open file\n";
        return;
    }

    HANDLE hMap = CreateFileMappingA(hFile, NULL, PAGE_READONLY, 0, 0, NULL);
    if (!hMap) {
        std::cerr << "Failed to create file mapping\n";
        CloseHandle(hFile);
        return;
    }

    char* data = (char*)MapViewOfFile(hMap, FILE_MAP_READ, 0, 0, 0);
    if (!data) {
        std::cerr << "Failed to map view of file\n";
        CloseHandle(hMap);
        CloseHandle(hFile);
        return;
    }

    int iterator = 0;
    while (true) {
        ++iterator;
        if (*data == '\0') break;
        float values[3];
        for (int i = 0; i <= 2; ++i) {
            values[i] = parseFloat4Decimal(data);
        }
        // if (iterator%400000 == 0) std::cout << values[0] << " " << values[1] << " " <<  values[2] << "\n";

        size_t cell = grid::fetch_cell(values[0], values[2]);
        particles.emplace_back(values[0], values[1], values[2], 1, 1, 1, cell);
    }


    UnmapViewOfFile(data);
    CloseHandle(hMap);
    CloseHandle(hFile);
}

}

