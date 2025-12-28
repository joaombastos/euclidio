#ifndef PRESET_MANAGER_H
#define PRESET_MANAGER_H

#include <Arduino.h>

// Forward declarations para evitar includes circulares
class EuclideanSequencer;
class EuclideanHarmonicSequencer;

class PresetManager {
public:
    // Paths para o cartão SD
    static constexpr const char* PRESETS_DIR = "/presets";
    static constexpr const char* EUCLID_PRESETS_DIR = "/presets/euclidean";
    static constexpr const char* HARMONIC_PRESETS_DIR = "/presets/harmonic";

    // Inicializa as pastas no SD
    static bool initDirectories();

    // Guardar presets dos dois sequenciadores
    static bool saveEuclideanPreset(EuclideanSequencer* seq, const char* presetName);
    static bool saveHarmonicPreset(EuclideanHarmonicSequencer* seq, const char* presetName);

    // Carregar presets
    static bool loadEuclideanPreset(EuclideanSequencer* seq, const char* presetName);
    static bool loadHarmonicPreset(EuclideanHarmonicSequencer* seq, const char* presetName);

    // Listar presets disponíveis
    static bool listEuclideanPresets(String* outList[], uint8_t maxCount, uint8_t &count);
    static bool listHarmonicPresets(String* outList[], uint8_t maxCount, uint8_t &count);

    // Apagar preset
    static bool deleteEuclideanPreset(const char* presetName);
    static bool deleteHarmonicPreset(const char* presetName);
};

#endif // PRESET_MANAGER_H
