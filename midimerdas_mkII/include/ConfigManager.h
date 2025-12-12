#ifndef CONFIG_MANAGER_H
#define CONFIG_MANAGER_H

#include <Arduino.h>

// Classe mínima mantida apenas para compatibilidade com código existente
// Toda a lógica de presets foi removida
class ConfigManager {
public:
  ConfigManager();
  
  // Inicializa SPIFFS (se necessário no futuro)
  void begin();
};

#endif
