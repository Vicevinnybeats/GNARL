/* Copyright 2013-2019 Matt Tytel
 *
 * vital is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * vital is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with vital.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Modified by Gnarl Audio, 2026: --bits, --block, --start and --save; the
 * renderer exits with an error when a named patch cannot be loaded.
 */

#include "JuceHeader.h"
#include "load_save.h"
#include "tuning.h"
#include "synth_base.h"

String getArgumentValue(int argc, const char* argv[], const String& flag, const String& full_flag) {
  for (int i = 0; i < argc - 1; ++i) {
    std::string arg = argv[i];
    if (arg == flag || arg == full_flag)
      return argv[i + 1];
  }
  
  return "";
}

bool hasFlag(int argc, const char* argv[], const String& flag, const String& full_flag) {
  for (int i = 0; i < argc - 1; ++i) {
    std::string arg = argv[i];
    if (arg == flag || arg == full_flag)
      return true;
  }

  return false;
}

float getRenderLength(int argc, const char* argv[]) {
  static constexpr float kDefaultRenderLength = 5.0f;
  // GNARL: 60, up from 15 - comparing against a reference drop needs more
  // than one 16-bar phrase at riddim tempo.
  static constexpr float kMaxRenderLength = 60.0f;
  
  String string_length = getArgumentValue(argc, argv, "-l", "--length");
  float length = kDefaultRenderLength;
  if (string_length.isEmpty())
    return kDefaultRenderLength;
  
  float float_val = string_length.getFloatValue();
  if (float_val > 0.0f)
    length = std::min(float_val, kMaxRenderLength);
  
  return length;
}

std::vector<int> getRenderMidiNotes(int argc, const char* argv[]) {
  static constexpr int kDefaultMidiNote = 48;
  
  String string_midi = getArgumentValue(argc, argv, "-m", "--midi");
  std::vector<int> midi_notes;
  if (!string_midi.isEmpty()) {
    StringArray midi_tokens;
    midi_tokens.addTokens(string_midi, ",", "");
    
    for (const String& midi_token : midi_tokens) {
      int midi = Tuning::noteToMidiKey(midi_token);
      if (midi >= 0)
        midi_notes.push_back(midi);
    }
  }
  
  if (midi_notes.empty())
    midi_notes.push_back(kDefaultMidiNote);
  
  return midi_notes;
}

float getRenderBpm(int argc, const char* argv[]) {
  static constexpr float kDefaultBpm = 120.0f;
  static constexpr float kMinBpm = 5.0f;
  static constexpr float kMaxBpm = 900.0f;

  String string_length = getArgumentValue(argc, argv, "-b", "--bpm");
  float bpm = kDefaultBpm;
  if (string_length.isEmpty())
    return kDefaultBpm;

  bpm = std::min(string_length.getFloatValue(), kMaxBpm);
  return std::max(bpm, kMinBpm);
}

void doRenderToFile(HeadlessSynth& headless_synth, int argc, const char* argv[]) {
  String string_output_file = getArgumentValue(argc, argv, "-o", "--output");
  bool render_images = hasFlag(argc, argv, "-i", "--render-images");

  if (string_output_file.isEmpty())
    return;
  
  if (!string_output_file.startsWith("/"))
    string_output_file = "./" + string_output_file;
  
  File output_file(string_output_file);
  if (!output_file.hasWriteAccess()) {
    std::cout << "Error: Don't have permission to write output file." << newLine;
    return;
  }

  float length = getRenderLength(argc, argv);
  float bpm = getRenderBpm(argc, argv);
  std::vector<int> midi_notes = getRenderMidiNotes(argc, argv);
  
  // GNARL: --bits 32 writes float (tests measure below 16-bit's floor);
  // --block N renders in N-sample blocks (1..kMaxBufferSize), so a test can
  // assert the output does not depend on how it was chunked.
  int bits = 16;
  String string_bits = getArgumentValue(argc, argv, "--bits", "--bits");
  if (string_bits == "24" || string_bits == "32")
    bits = string_bits.getIntValue();

  int block_size = 64;
  String string_block = getArgumentValue(argc, argv, "--block", "--block");
  if (string_block.isNotEmpty())
    block_size = string_block.getIntValue();

  // GNARL: --start S puts note-on at transport position S seconds.
  double start_seconds = 0.0;
  String string_start = getArgumentValue(argc, argv, "--start", "--start");
  if (string_start.isNotEmpty())
    start_seconds = string_start.getDoubleValue();

  headless_synth.renderAudioToFile(output_file, length, bpm, midi_notes, render_images, bits, block_size,
                                   start_seconds);
}

// GNARL: --save FILE writes the loaded (or init) patch back out as a complete
// preset, through the engine's own save path. Test patches start from one of
// these and change only what the test needs, so they can never be missing a
// section the loader expects.
void doSaveToFile(HeadlessSynth& headless_synth, int argc, const char* argv[]) {
  String string_save_file = getArgumentValue(argc, argv, "--save", "--save");
  if (string_save_file.isEmpty())
    return;

  if (!string_save_file.startsWith("/"))
    string_save_file = "./" + string_save_file;

  if (!headless_synth.saveToFile(File(string_save_file)))
    std::cout << "Error: could not save preset." << newLine;
}

bool loadFromCommandLine(HeadlessSynth& synth, const String& command_line) {
  String file_path = command_line;
  if (file_path[0] == '"' && file_path[file_path.length() - 1] == '"')
    file_path = command_line.substring(1, command_line.length() - 1);
  File file = File::getCurrentWorkingDirectory().getChildFile(file_path);

  // GNARL: a patch that is named but cannot be loaded is an ERROR, not a
  // skip. Upstream ignored the failure and rendered whatever was already
  // loaded (the init patch), so a patch stamped with a newer synth_version,
  // which jsonToState refuses, rendered as init with exit code 0 - and every
  // measurement taken from it measured the wrong sound.
  if (!file.exists()) {
    if (file.hasFileExtension(String(vital::kPresetExtension))) {
      std::cerr << "Error: no such patch: " << file.getFullPathName() << std::endl;
      std::exit(1);
    }
    return false;
  }

  std::string error;
  if (!synth.loadFromFile(file, error)) {
    std::cerr << "Error: could not load " << file.getFullPathName() << ": "
              << (error.empty() ? "unknown error" : error) << std::endl;
    std::exit(1);
  }
  return true;
}

int main(int argc, const char* argv[]) {
  HeadlessSynth headless_synth;
  
  bool last_arg_was_option = false;
  for (int i = 1; i < argc; ++i) {
    std::string arg = argv[i];
    if (arg != "" && arg[0] != '-' && !last_arg_was_option && loadFromCommandLine(headless_synth, arg))
      break;

    last_arg_was_option = arg[0] == '-' && arg != "--headless";
  }
  
  doSaveToFile(headless_synth, argc, argv);
  doRenderToFile(headless_synth, argc, argv);
}
