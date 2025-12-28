#include "PresetManager.h"
#include "SdCard.h"
#include "EuclideanSequencer.h"
#include "EuclideanHarmonicSequencer.h"
#include <SD.h>

bool PresetManager::initDirectories() {
    if (!SdCardManager::begin()) {
        return false;
    }

    // Criar pasta raiz /presets
    if (!SD.exists(PRESETS_DIR)) {
        if (!SD.mkdir(PRESETS_DIR)) {
            return false;
        }
    }

    // Criar pasta /presets/euclidean
    if (!SD.exists(EUCLID_PRESETS_DIR)) {
        if (!SD.mkdir(EUCLID_PRESETS_DIR)) {
            return false;
        }
    }

    // Criar pasta /presets/harmonic
    if (!SD.exists(HARMONIC_PRESETS_DIR)) {
        if (!SD.mkdir(HARMONIC_PRESETS_DIR)) {
            return false;
        }
    }

    // Recriar sempre o preset "default" do euclidiano com 8 tracks explícitas em estado de fábrica
    // (steps=8, hits=4, offset=0, note=36, velocity=100, midiChannel=1, resolution=2, noteLength=100, enabled=false)
    {
        String defaultEucPath = String(EUCLID_PRESETS_DIR) + "/default.json";
        String json = "{\n";
        json += "  \"type\": \"euclidean\",\n";
        json += "  \"version\": 1,\n";
        json += "  \"tracks\": [\n";
        for (uint8_t t = 0; t < 8; t++) {
            json += "    {\n";
            json += "      \"trackIndex\": " + String(t) + ",\n";
            json += "      \"steps\": 8,\n";
            json += "      \"hits\": 4,\n";
            json += "      \"offset\": 0,\n";
            json += "      \"note\": 36,\n";
            json += "      \"velocity\": 100,\n";
            json += "      \"midiChannel\": 1,\n";
            json += "      \"resolution\": 2,\n";
            json += "      \"noteLength\": 100,\n";
            json += "      \"enabled\": false\n";
            json += "    }";
            if (t < 7) json += ",\n"; else json += "\n";
        }
        json += "  ]\n";
        json += "}\n";
        SdCardManager::writeText(defaultEucPath.c_str(), json, false);
    }

    String defaultHarmPath = String(HARMONIC_PRESETS_DIR) + "/default.json";
    if (!SD.exists(defaultHarmPath.c_str())) {
        // Criar preset harmónico default (vazio/padrão)
        String json = "{\n  \"type\": \"harmonic\",\n  \"version\": 1,\n  \"tracks\": []\n}\n";
        SdCardManager::writeText(defaultHarmPath.c_str(), json, false);
    }

    return true;
}

bool PresetManager::saveEuclideanPreset(EuclideanSequencer* seq, const char* presetName) {
    if (!seq || !presetName) return false;
    if (!initDirectories()) return false;

    // Construir o JSON com os dados do sequenciador
    String json = "{\n";
    json += "  \"type\": \"euclidean\",\n";
    json += "  \"version\": 1,\n";
    json += "  \"tracks\": [\n";

    // Guardar dados de cada track (0-7)
    for (uint8_t t = 0; t < 8; t++) {
        json += "    {\n";
        json += "      \"trackIndex\": " + String(t) + ",\n";
        json += "      \"steps\": " + String(seq->getTrackSteps(t)) + ",\n";
        json += "      \"hits\": " + String(seq->getTrackHits(t)) + ",\n";
        json += "      \"offset\": " + String(seq->getTrackOffset(t)) + ",\n";
        json += "      \"note\": " + String(seq->getTrackNote(t)) + ",\n";
        json += "      \"velocity\": " + String(seq->getTrackVelocity(t)) + ",\n";
        json += "      \"midiChannel\": " + String(seq->getTrackMidiChannel(t)) + ",\n";
        json += "      \"resolution\": " + String(seq->getTrackResolution(t)) + ",\n";
        json += "      \"noteLength\": " + String(seq->getTrackNoteLength(t)) + ",\n";
        json += "      \"enabled\": " + String(seq->isTrackEnabled(t) ? "true" : "false") + "\n";
        json += "    }";
        if (t < 7) json += ",";
        json += "\n";
    }

    json += "  ],\n";
    json += "  \"outputNotes\": " + String(seq->getOutputNotes()) + ",\n";
    json += "  \"outputClock\": " + String(seq->getOutputClock()) + ",\n";
    json += "  \"outputMidiMap\": " + String(seq->getOutputMidiMap() ? "true" : "false") + ",\n";
    json += "  \"outputOSCMap\": " + String(seq->getOutputOSCMap() ? "true" : "false") + "\n";
    json += "}\n";

    // Guardar no SD
    String filePath = String(EUCLID_PRESETS_DIR) + "/" + String(presetName) + ".json";
    return SdCardManager::writeText(filePath.c_str(), json, false);
}

bool PresetManager::loadEuclideanPreset(EuclideanSequencer* seq, const char* presetName) {
    if (!seq || !presetName) return false;
    if (!SdCardManager::begin()) return false;

    String filePath = String(EUCLID_PRESETS_DIR) + "/" + String(presetName) + ".json";
    String json;
    if (!SdCardManager::readText(filePath.c_str(), json)) {
        return false;
    }

    // Parse JSON simples (sem biblioteca externa, apenas parsing manual)
    // Formato esperado: \"fieldName\": value
    
    for (uint8_t t = 0; t < 8; t++) {
        // Procura pelos índices das tracks no JSON
        String trackStr = "\"trackIndex\": " + String(t);
        int trackIdx = json.indexOf(trackStr);
        if (trackIdx < 0) continue;

        // Extrai o bloco da track (entre { e })
        int blockStart = json.lastIndexOf("{", trackIdx);
        int blockEnd = json.indexOf("}", trackIdx);
        if (blockStart < 0 || blockEnd < 0) continue;

        String blockJson = json.substring(blockStart, blockEnd + 1);

        // Extrai cada valor
        auto extractInt = [](const String& s, const String& key) -> int {
            int idx = s.indexOf(key);
            if (idx < 0) return -1;
            idx = s.indexOf(":", idx) + 1;
            while (idx < s.length() && (s[idx] == ' ' || s[idx] == '\n' || s[idx] == '\t')) idx++;
            int endIdx = idx;
            while (endIdx < s.length() && (isdigit(s[endIdx]) || s[endIdx] == '-')) endIdx++;
            return s.substring(idx, endIdx).toInt();
        };

        auto extractBool = [](const String& s, const String& key) -> bool {
            int idx = s.indexOf(key);
            if (idx < 0) return false;
            idx = s.indexOf(":", idx);
            String sub = s.substring(idx);
            return sub.indexOf("true") >= 0;
        };

        int steps = extractInt(blockJson, "\"steps\"");
        int hits = extractInt(blockJson, "\"hits\"");
        int offset = extractInt(blockJson, "\"offset\"");
        int note = extractInt(blockJson, "\"note\"");
        int velocity = extractInt(blockJson, "\"velocity\"");
        int midiChannel = extractInt(blockJson, "\"midiChannel\"");
        int resolution = extractInt(blockJson, "\"resolution\"");
        int noteLength = extractInt(blockJson, "\"noteLength\"");
        bool enabled = extractBool(blockJson, "\"enabled\"");

        // Aplicar à track selecionada
        seq->setSelectedPattern(t);
        seq->loadPatternConfig();
        
        if (steps > 0) seq->setSteps(steps);
        if (hits >= 0) seq->setHits(hits);
        if (offset >= 0) seq->setOffset(offset);
        if (note >= 0) seq->setNote(note);
        if (velocity >= 0) seq->setVelocity(velocity);
        if (midiChannel >= 0) seq->setMidiChannel(midiChannel);
        if (resolution > 0) seq->setResolution(resolution);
        if (noteLength > 0) seq->setNoteLength(noteLength);
        seq->setTrackEnabled(t, enabled);

        seq->savePattern(t);
    }

    return true;
}

bool PresetManager::saveHarmonicPreset(EuclideanHarmonicSequencer* seq, const char* presetName) {
    if (!seq || !presetName) return false;
    if (!initDirectories()) return false;

    String json = "{\n";
    json += "  \"type\": \"harmonic\",\n";
    json += "  \"version\": 1,\n";
    json += "  \"tracks\": [\n";

    // Guardar dados de cada track (0-7, mas apresentados como 1-8)
    for (uint8_t t = 0; t < 8; t++) {
        seq->setActiveTrack(t + 1);  // 1-based
        
        json += "    {\n";
        json += "      \"trackIndex\": " + String(t) + ",\n";
        json += "      \"steps\": " + String(seq->getSteps()) + ",\n";
        json += "      \"hits\": " + String(seq->getHits()) + ",\n";
        json += "      \"offset\": " + String(seq->getOffset()) + ",\n";
        json += "      \"tonic\": " + String(seq->getTonic()) + ",\n";
        json += "      \"majorScale\": " + String(seq->isMajorScale() ? "true" : "false") + ",\n";
        json += "      \"baseOctave\": " + String(seq->getBaseOctave()) + ",\n";
        json += "      \"polyphony\": " + String(seq->getPolyphony()) + ",\n";
        json += "      \"midiChannel\": " + String(seq->getMidiChannel()) + ",\n";
        json += "      \"velocity\": " + String(seq->getVelocity()) + ",\n";
        json += "      \"noteLength\": " + String(seq->getNoteLength()) + ",\n";
        json += "      \"distributionMode\": " + String(seq->getDistributionMode()) + ",\n";
        json += "      \"scaleType\": " + String(seq->getScaleType()) + ",\n";
        json += "      \"resolutionIndex\": " + String(seq->getResolutionIndex()) + ",\n";
        json += "      \"enabled\": " + String(seq->isTrackEnabled(t) ? "true" : "false") + ",\n";
        
        // Salvar lista de acordes (graus da escala)
        uint8_t chordListSize = seq->getChordListSize();
        json += "      \"chordList\": [";
        for (uint8_t c = 0; c < chordListSize; c++) {
            json += String(seq->getChordListItem(c));
            if (c < chordListSize - 1) json += ", ";
        }
        json += "]\n";
        json += "    }";
        if (t < 7) json += ",";
        json += "\n";
    }

    json += "  ],\n";
    json += "  \"outputMidiMap\": " + String(seq->getOutputMidiMap() ? "true" : "false") + ",\n";
    json += "  \"outputOSCMap\": " + String(seq->getOutputOSCMap() ? "true" : "false") + "\n";
    json += "}\n";

    // Guardar no SD
    String filePath = String(HARMONIC_PRESETS_DIR) + "/" + String(presetName) + ".json";
    return SdCardManager::writeText(filePath.c_str(), json, false);
}

bool PresetManager::loadHarmonicPreset(EuclideanHarmonicSequencer* seq, const char* presetName) {
    if (!seq || !presetName) return false;
    if (!SdCardManager::begin()) return false;

    String filePath = String(HARMONIC_PRESETS_DIR) + "/" + String(presetName) + ".json";
    String json;
    if (!SdCardManager::readText(filePath.c_str(), json)) {
        return false;
    }

    // Parse JSON simples (sem biblioteca externa)
    
    auto extractInt = [](const String& s, const String& key) -> int {
        int idx = s.indexOf(key);
        if (idx < 0) return -1;
        idx = s.indexOf(":", idx) + 1;
        while (idx < s.length() && (s[idx] == ' ' || s[idx] == '\n' || s[idx] == '\t')) idx++;
        int endIdx = idx;
        while (endIdx < s.length() && (isdigit(s[endIdx]) || s[endIdx] == '-')) endIdx++;
        return s.substring(idx, endIdx).toInt();
    };

    auto extractBool = [](const String& s, const String& key) -> bool {
        int idx = s.indexOf(key);
        if (idx < 0) return false;
        idx = s.indexOf(":", idx);
        String sub = s.substring(idx);
        return sub.indexOf("true") >= 0;
    };

    for (uint8_t t = 0; t < 8; t++) {
        String trackStr = "\"trackIndex\": " + String(t);
        int trackIdx = json.indexOf(trackStr);
        if (trackIdx < 0) continue;

        int blockStart = json.lastIndexOf("{", trackIdx);
        int blockEnd = json.indexOf("}", trackIdx);
        if (blockStart < 0 || blockEnd < 0) continue;

        String blockJson = json.substring(blockStart, blockEnd + 1);

        seq->setActiveTrack(t + 1);  // 1-based

        int steps = extractInt(blockJson, "\"steps\"");
        int hits = extractInt(blockJson, "\"hits\"");
        int offset = extractInt(blockJson, "\"offset\"");
        int tonic = extractInt(blockJson, "\"tonic\"");
        int baseOctave = extractInt(blockJson, "\"baseOctave\"");
        int polyphony = extractInt(blockJson, "\"polyphony\"");
        int midiChannel = extractInt(blockJson, "\"midiChannel\"");
        int velocity = extractInt(blockJson, "\"velocity\"");
        int noteLength = extractInt(blockJson, "\"noteLength\"");
        int distMode = extractInt(blockJson, "\"distributionMode\"");
        int scaleType = extractInt(blockJson, "\"scaleType\"");
        int resIdx = extractInt(blockJson, "\"resolutionIndex\"");
        bool enabled = extractBool(blockJson, "\"enabled\"");

        // Aplicar
        if (steps > 0) seq->setSteps(steps);
        if (hits >= 0) seq->setHits(hits);
        if (offset >= 0) seq->setOffset(offset);
        if (tonic >= 0) seq->setTonic(tonic);
        if (baseOctave >= -2 && baseOctave <= 2) seq->setBaseOctave(baseOctave);
        if (polyphony > 0) seq->setPolyphony(polyphony);
        if (midiChannel >= 0) seq->setMidiChannel(midiChannel);
        if (velocity >= 0) seq->setVelocity(velocity);
        if (noteLength > 0) seq->setNoteLength(noteLength);
        if (distMode >= 0) seq->setDistributionMode(distMode);
        if (scaleType >= 0) seq->setScaleType((EuclideanHarmonicSequencer::ScaleType)scaleType);
        if (resIdx >= 0) seq->setResolutionIndex(resIdx);
    }

    return true;
}

bool PresetManager::listEuclideanPresets(String* outList[], uint8_t maxCount, uint8_t &count) {
    if (!SdCardManager::begin()) return false;

    File dir = SD.open(EUCLID_PRESETS_DIR);
    if (!dir || !dir.isDirectory()) return false;

    count = 0;
    File file = dir.openNextFile();
    while (file && count < maxCount) {
        if (!file.isDirectory()) {
            String name = file.name();
            if (name.endsWith(".json")) {
                name = name.substring(0, name.length() - 5); // remove .json
                if (outList[count]) {
                    *outList[count] = name;
                }
                count++;
            }
        }
        file = dir.openNextFile();
    }
    dir.close();
    return true;
}

bool PresetManager::listHarmonicPresets(String* outList[], uint8_t maxCount, uint8_t &count) {
    if (!SdCardManager::begin()) return false;

    File dir = SD.open(HARMONIC_PRESETS_DIR);
    if (!dir || !dir.isDirectory()) return false;

    count = 0;
    File file = dir.openNextFile();
    while (file && count < maxCount) {
        if (!file.isDirectory()) {
            String name = file.name();
            if (name.endsWith(".json")) {
                name = name.substring(0, name.length() - 5); // remove .json
                if (outList[count]) {
                    *outList[count] = name;
                }
                count++;
            }
        }
        file = dir.openNextFile();
    }
    dir.close();
    return true;
}

bool PresetManager::deleteEuclideanPreset(const char* presetName) {
    if (!SdCardManager::begin()) return false;

    String filePath = String(EUCLID_PRESETS_DIR) + "/" + String(presetName) + ".json";
    if (SD.exists(filePath.c_str())) {
        return SD.remove(filePath.c_str());
    }
    return false;
}

bool PresetManager::deleteHarmonicPreset(const char* presetName) {
    if (!SdCardManager::begin()) return false;

    String filePath = String(HARMONIC_PRESETS_DIR) + "/" + String(presetName) + ".json";
    if (SD.exists(filePath.c_str())) {
        return SD.remove(filePath.c_str());
    }
    return false;
}
