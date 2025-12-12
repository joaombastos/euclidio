#include "ConfigManager.h"
#include <SPIFFS.h>

ConfigManager::ConfigManager() {
}

void ConfigManager::begin() {
  // Inicializa SPIFFS
  if (!SPIFFS.begin(true)) {
    return;
  }
}

