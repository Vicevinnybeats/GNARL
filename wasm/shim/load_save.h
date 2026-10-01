// GNARL: the browser build's stand-in for src/common/load_save.h
// (wasm/README.md). The only includer it compiles is wavetable_creator.cpp,
// which needs LoadSave's version comparison and two PCM conversions to
// migrate old wavetable JSON. Those three are copied here without JUCE's
// String; the real LoadSave needs SynthBase and files, which the web build
// does not have.
#pragma once

#include "JuceHeader.h"
#include "json/json.h"
#include "synth_constants.h"
#include "utils.h"

#include <cstdlib>
#include <string>

using json = nlohmann::json;

namespace ProjectInfo {
  // The build passes the plugin's version (wasm/build.sh reads the .jucer).
  const char* const versionString = GNARL_VERSION;
}

class LoadSave {
  public:
    static int compareVersionStrings(std::string a, std::string b) {
      auto trim = [](std::string s) {
        size_t start = s.find_first_not_of(" \t\r\n");
        size_t end = s.find_last_not_of(" \t\r\n");
        return start == std::string::npos ? std::string() : s.substr(start, end - start + 1);
      };
      a = trim(a);
      b = trim(b);
      if (a.empty() && b.empty())
        return 0;

      auto split = [](const std::string& s, std::string& rest) {
        size_t dot = s.find('.');
        std::string major = dot == std::string::npos ? s : s.substr(0, dot);
        rest = dot == std::string::npos ? std::string() : s.substr(dot + 1);
        if (major.empty() || major.find_first_not_of("0123456789") != std::string::npos)
          major = "0";
        return std::atoi(major.c_str());
      };
      std::string rest_a, rest_b;
      int major_a = split(a, rest_a);
      int major_b = split(b, rest_b);
      if (major_a > major_b)
        return 1;
      if (major_a < major_b)
        return -1;
      return compareVersionStrings(rest_a, rest_b);
    }

    // As LoadSave::compareFeatureVersionStrings: major.minor only.
    static int compareFeatureVersionStrings(std::string a, std::string b) {
      auto feature = [](const std::string& s) {
        size_t dot = s.rfind('.');
        return dot == std::string::npos ? s : s.substr(0, dot);
      };
      return compareVersionStrings(feature(a), feature(b));
    }

    // A copy of LoadSave::readableNewerPatch (src/common/load_save.cpp): keep
    // the two identical. tests/test_web.py loads the same patches in both.
    static bool readableNewerPatch(const json& data) {
      if (compareFeatureVersionStrings(data["synth_version"].get<std::string>(), "1.5.0") > 0)
        return false;
      const json& settings = data["settings"];
      for (int i = 1; i <= vital::kNumOscillators; ++i) {
        std::string number = std::to_string(i);
        std::string phase = "osc_" + number + "_spectral_morph_phase";
        if (settings.count(phase) && std::abs(settings[phase].get<float>() - 0.5f) > 1e-6f)
          return false;
        std::string warp = "osc_" + number + "_distortion_type";
        if (settings.count(warp) && settings[warp].get<float>() >= kNumWarpTypes)
          return false;
      }
      return true;
    }

    // SynthOscillator::kNumDistortionTypes, which this header cannot include.
    static constexpr int kNumWarpTypes = 13;

    static void convertBufferToPcm(json& data, const std::string& field) {
      if (data.count(field) == 0)
        return;

      MemoryOutputStream decoded;
      std::string wave_data = data[field];
      Base64::convertFromBase64(decoded, wave_data);
      int size = static_cast<int>(decoded.getDataSize()) / sizeof(float);
      std::unique_ptr<float[]> float_data = std::make_unique<float[]>(size);
      memcpy(float_data.get(), decoded.getData(), size * sizeof(float));
      std::unique_ptr<int16_t[]> pcm_data = std::make_unique<int16_t[]>(size);
      vital::utils::floatToPcmData(pcm_data.get(), float_data.get(), size);
      data[field] = Base64::toBase64(pcm_data.get(), sizeof(int16_t) * size).toStdString();
    }

    static void convertPcmToFloatBuffer(json& data, const std::string& field) {
      if (data.count(field) == 0)
        return;

      MemoryOutputStream decoded;
      std::string wave_data = data[field];
      Base64::convertFromBase64(decoded, wave_data);
      int size = static_cast<int>(decoded.getDataSize()) / sizeof(int16_t);
      std::unique_ptr<int16_t[]> pcm_data = std::make_unique<int16_t[]>(size);
      memcpy(pcm_data.get(), decoded.getData(), size * sizeof(int16_t));
      std::unique_ptr<float[]> float_data = std::make_unique<float[]>(size);
      vital::utils::pcmToFloatData(float_data.get(), pcm_data.get(), size);
      data[field] = Base64::toBase64(float_data.get(), sizeof(float) * size).toStdString();
    }
};
