#include "SdCard.h"

#include <SPI.h>
#include <SD.h>

namespace {
    // Pinos fixos fornecidos pelo utilizador
    constexpr int SD_CS_PIN   = 21;  // CS
    constexpr int SD_SCK_PIN  = 14;  // SCK
    constexpr int SD_MISO_PIN = 6;   // MISO
    constexpr int SD_MOSI_PIN = 13;  // MOSI

    bool sdMounted = false;
}

bool SdCardManager::begin() {
    if (sdMounted) {
        return true;
    }

    // Inicialização obrigatória indicada pelo utilizador
    SPI.begin(SD_SCK_PIN, SD_MISO_PIN, SD_MOSI_PIN, SD_CS_PIN);

    if (!SD.begin(SD_CS_PIN)) {
        sdMounted = false;
        return false;
    }

    sdMounted = true;
    return true;
}

bool SdCardManager::isMounted() {
    return sdMounted;
}

bool SdCardManager::writeText(const char *path, const String &content, bool append) {
    if (!begin()) {
        return false;
    }

    // A maioria das integrações SdFat/SD usa FILE_WRITE para escrita (normalmente append).
    // Para simplificar a compatibilidade, usamos sempre FILE_WRITE e, se for append,
    // garantimos que o cursor está no fim do ficheiro.
    File file = SD.open(path, FILE_WRITE);
    if (!file) {
        return false;
    }

    if (append) {
        file.seek(file.size());
    }

    // Evita dependência de Print::print(String) que pode não existir
    const char *cstr = content.c_str();
    size_t len = content.length();
    size_t written = file.write(reinterpret_cast<const uint8_t *>(cstr), len);
    file.close();

    return written == content.length();
}

bool SdCardManager::readText(const char *path, String &out) {
    if (!begin()) {
        return false;
    }

    File file = SD.open(path, FILE_READ);
    if (!file) {
        return false;
    }

    out = "";
    while (file.available()) {
        out += static_cast<char>(file.read());
    }

    file.close();
    return true;
}
