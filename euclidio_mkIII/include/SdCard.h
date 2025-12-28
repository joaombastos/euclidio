#ifndef SDCARD_MANAGER_H
#define SDCARD_MANAGER_H

#include <Arduino.h>

class SdCardManager {
public:
    // Inicializa o barramento SPI e o cartão SD.
    // Usa sempre os mesmos pinos e CS definidos no .cpp.
    // Retorna true em caso de sucesso.
    static bool begin();

    // Indica se o cartão foi montado com sucesso.
    static bool isMounted();

    // Escreve uma string de texto num ficheiro.
    // Se append == true, acrescenta ao fim do ficheiro; caso contrário, reescreve.
    static bool writeText(const char *path, const String &content, bool append = false);

    // Lê todo o conteúdo de um ficheiro de texto para "out".
    static bool readText(const char *path, String &out);
};

#endif // SDCARD_MANAGER_H
