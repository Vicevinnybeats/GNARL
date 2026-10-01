// tools/vst3_probe.cpp - loads a VST3 the way a host does and reports it.
//
//   g++ -std=c++17 -I third_party/VST_SDK/VST3_SDK tools/vst3_probe.cpp -o probe -ldl
//   ./probe /abs/path/GNARL.so            factory: vendor, classes, class IDs
//   ./probe /abs/path/GNARL.so --params   also instantiates the plugin and lists
//                                         every parameter in HOST order
//   ./probe /abs/path/GNARL.so --render OUT.f32 [ID=VALUE,...]
//                                         plays C2 for 2 s (4 s rendered) at
//                                         48 kHz, 140 BPM, 256-sample blocks,
//                                         and writes interleaved stereo float32;
//                                         each ID=VALUE (host parameter ID from
//                                         --params, normalised 0..1) is sent as
//                                         a parameter change in the first block;
//                                         offset=N starts the note N samples
//                                         into that block, as a host does when
//                                         a note falls between block starts
//   GNARL_PROBE_STATE=patch.vital ./probe ... --render OUT.f32
//                                         first hands the plugin a patch as
//                                         its state (IComponent::setState), as
//                                         a DAW restoring a project does -
//                                         the path the panel's starting
//                                         sounds take (WebPanel::loadFactory)
//
// --render drives the plugin exactly as a host does - setupProcessing, bus
// activation, a transport, note events - so two builds of the plugin can be
// compared sample for sample (the JUCE 6 -> 8 move, phase2-03-juce8.md). It
// exercises the JUCE VST3 wrapper, which the offline renderer never touches.
//
// Compiling is not loading. A plugin can build and link and still fail here,
// which is the only step a DAW's scanner cares about - and the parameter
// order printed by --params is the order a DAW keys automation lanes by, so
// a parameter added anywhere but the END moves every lane after it.
#define INIT_CLASS_IID
#include <dlfcn.h>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <string>
#include <algorithm>
#include "pluginterfaces/base/ipluginbase.h"
#include "pluginterfaces/vst/ivstcomponent.h"
#include "pluginterfaces/vst/ivsteditcontroller.h"
#include "pluginterfaces/vst/ivstaudioprocessor.h"
#include "pluginterfaces/vst/ivstmessage.h"
#include "pluginterfaces/vst/ivstevents.h"
#include "pluginterfaces/vst/ivstprocesscontext.h"
#include "pluginterfaces/vst/ivstparameterchanges.h"
#include "pluginterfaces/base/ibstream.h"
#include <fstream>
#include <iterator>
#include <vector>
#include <vector>

using namespace Steinberg;
using namespace Steinberg::Vst;

static std::string utf8(const TChar* s) {
  std::string out;
  for (; *s; ++s) out += (*s < 128) ? static_cast<char>(*s) : '?';
  return out;
}

// The smallest IEventList a host can hand over: a fixed list of events.
class EventList : public IEventList {
  public:
    std::vector<Event> events;
    tresult PLUGIN_API queryInterface(const TUID iid, void** obj) override {
      if (FUnknownPrivate::iidEqual(iid, IEventList::iid) || FUnknownPrivate::iidEqual(iid, FUnknown::iid)) {
        *obj = this;
        return kResultOk;
      }
      *obj = nullptr;
      return kNoInterface;
    }
    uint32 PLUGIN_API addRef() override { return 1; }
    uint32 PLUGIN_API release() override { return 1; }
    int32 PLUGIN_API getEventCount() override { return (int32) events.size(); }
    tresult PLUGIN_API getEvent(int32 index, Event& e) override {
      if (index < 0 || index >= (int32) events.size()) return kInvalidArgument;
      e = events[index];
      return kResultOk;
    }
    tresult PLUGIN_API addEvent(Event& e) override { events.push_back(e); return kResultOk; }
};

// One point per parameter, at sample 0: how a host sends a value it set
// before playback started.
class ValueQueue : public IParamValueQueue {
  public:
    ValueQueue(ParamID id, ParamValue value) : id_(id), value_(value) { }
    tresult PLUGIN_API queryInterface(const TUID iid, void** obj) override {
      if (FUnknownPrivate::iidEqual(iid, IParamValueQueue::iid) || FUnknownPrivate::iidEqual(iid, FUnknown::iid)) {
        *obj = this;
        return kResultOk;
      }
      *obj = nullptr;
      return kNoInterface;
    }
    uint32 PLUGIN_API addRef() override { return 1; }
    uint32 PLUGIN_API release() override { return 1; }
    ParamID PLUGIN_API getParameterId() override { return id_; }
    int32 PLUGIN_API getPointCount() override { return 1; }
    tresult PLUGIN_API getPoint(int32 index, int32& offset, ParamValue& value) override {
      if (index != 0) return kInvalidArgument;
      offset = 0;
      value = value_;
      return kResultOk;
    }
    tresult PLUGIN_API addPoint(int32, ParamValue, int32&) override { return kNotImplemented; }
  private:
    ParamID id_;
    ParamValue value_;
};

class ParameterChanges : public IParameterChanges {
  public:
    std::vector<ValueQueue> queues;
    tresult PLUGIN_API queryInterface(const TUID iid, void** obj) override {
      if (FUnknownPrivate::iidEqual(iid, IParameterChanges::iid) || FUnknownPrivate::iidEqual(iid, FUnknown::iid)) {
        *obj = this;
        return kResultOk;
      }
      *obj = nullptr;
      return kNoInterface;
    }
    uint32 PLUGIN_API addRef() override { return 1; }
    uint32 PLUGIN_API release() override { return 1; }
    int32 PLUGIN_API getParameterCount() override { return (int32) queues.size(); }
    IParamValueQueue* PLUGIN_API getParameterData(int32 index) override {
      return index >= 0 && index < (int32) queues.size() ? &queues[index] : nullptr;
    }
    IParamValueQueue* PLUGIN_API addParameterData(const ParamID&, int32&) override { return nullptr; }
};

static Event noteEvent(bool on, int32 offset) {
  Event e = {};
  e.busIndex = 0;
  e.sampleOffset = offset;
  e.type = on ? Event::kNoteOnEvent : Event::kNoteOffEvent;
  if (on) { e.noteOn.channel = 0; e.noteOn.pitch = 36; e.noteOn.velocity = 1.0f; e.noteOn.noteId = -1; }
  else { e.noteOff.channel = 0; e.noteOff.pitch = 36; e.noteOff.velocity = 0.0f; e.noteOff.noteId = -1; }
  return e;
}

static int render(IComponent* component, const char* path, const char* settings) {
  ParameterChanges initial;
  int32 note_offset = 0;
  for (const char* p = settings; p && *p;) {
    int used_offset = 0;
    if (std::sscanf(p, "offset=%d%n", &note_offset, &used_offset) == 1) {
      p += used_offset;
      if (*p == ',') ++p;
      continue;
    }
    unsigned id = 0;
    double value = 0.0;
    int used = 0;
    if (std::sscanf(p, "%u=%lf%n", &id, &value, &used) != 2) { printf("bad setting: %s\n", p); return 2; }
    initial.queues.emplace_back(id, value);
    p += used;
    if (*p == ',') ++p;
  }
  ParameterChanges none;

  IAudioProcessor* processor = nullptr;
  component->queryInterface(IAudioProcessor::iid, (void**) &processor);
  if (!processor) { printf("no IAudioProcessor\n"); return 1; }

  constexpr double kRate = 48000.0;
  constexpr int32 kBlock = 256;
  constexpr int kBlocks = (int) (4.0 * kRate / kBlock);
  constexpr int kNoteOffBlock = (int) (2.0 * kRate / kBlock);
  if (note_offset < 0 || note_offset >= kBlock) { printf("offset must be 0..%d\n", kBlock - 1); return 2; }

  ProcessSetup setup = { kOffline, kSample32, kBlock, kRate };
  if (processor->setupProcessing(setup) != kResultOk) { printf("setupProcessing FAILED\n"); return 1; }
  SpeakerArrangement out_arrangement = SpeakerArr::kStereo;
  processor->setBusArrangements(nullptr, 0, &out_arrangement, 1);
  component->activateBus(kAudio, kOutput, 0, true);
  component->setActive(true);
  processor->setProcessing(true);

  std::vector<float> left(kBlock), right(kBlock), out;
  out.reserve((size_t) kBlocks * kBlock * 2);
  float* channels[2] = { left.data(), right.data() };
  AudioBusBuffers bus = {};
  bus.numChannels = 2;
  bus.channelBuffers32 = channels;

  ProcessContext context = {};
  context.state = ProcessContext::kPlaying | ProcessContext::kTempoValid | ProcessContext::kTimeSigValid
                  | ProcessContext::kProjectTimeMusicValid;
  context.sampleRate = kRate;
  context.tempo = 140.0;
  context.timeSigNumerator = 4;
  context.timeSigDenominator = 4;

  for (int b = 0; b < kBlocks; ++b) {
    EventList events;
    if (b == 0) events.events.push_back(noteEvent(true, note_offset));
    if (b == kNoteOffBlock) events.events.push_back(noteEvent(false, 0));

    context.projectTimeSamples = (int64) b * kBlock;
    context.projectTimeMusic = context.projectTimeSamples / kRate * (context.tempo / 60.0);

    ProcessData data;
    data.processMode = kOffline;
    data.symbolicSampleSize = kSample32;
    data.numSamples = kBlock;
    data.numInputs = 0;
    data.numOutputs = 1;
    data.outputs = &bus;
    data.inputEvents = &events;
    data.inputParameterChanges = b == 0 ? &initial : &none;
    data.processContext = &context;
    if (processor->process(data) != kResultOk) { printf("process FAILED at block %d\n", b); return 1; }
    for (int i = 0; i < kBlock; ++i) { out.push_back(left[i]); out.push_back(right[i]); }
  }

  processor->setProcessing(false);
  component->setActive(false);
  processor->release();

  FILE* file = std::fopen(path, "wb");
  if (!file) { printf("cannot write %s\n", path); return 1; }
  std::fwrite(out.data(), sizeof(float), out.size(), file);
  std::fclose(file);
  float peak = 0.0f;
  for (float v : out) peak = std::max(peak, v < 0 ? -v : v);
  printf("rendered %d frames, peak %.4f -> %s\n", kBlocks * kBlock, peak, path);
  return 0;
}

// A read-only IBStream over bytes in memory: enough for setState.
class MemoryStream : public IBStream {
 public:
  explicit MemoryStream(std::vector<char> data) : data_(std::move(data)) {}
  tresult PLUGIN_API queryInterface(const TUID iid, void** obj) override {
    if (FUnknownPrivate::iidEqual(iid, IBStream::iid) || FUnknownPrivate::iidEqual(iid, FUnknown::iid)) {
      *obj = this;
      return kResultOk;
    }
    *obj = nullptr;
    return kNoInterface;
  }
  uint32 PLUGIN_API addRef() override { return 1; }
  uint32 PLUGIN_API release() override { return 1; }
  tresult PLUGIN_API read(void* buffer, int32 bytes, int32* read_bytes) override {
    int32 n = std::max<int32>(0, std::min<int32>(bytes, (int32) data_.size() - (int32) pos_));
    std::memcpy(buffer, data_.data() + pos_, n);
    pos_ += n;
    if (read_bytes) *read_bytes = n;
    return kResultOk;
  }
  tresult PLUGIN_API write(void*, int32, int32*) override { return kNotImplemented; }
  tresult PLUGIN_API seek(int64 pos, int32 mode, int64* result) override {
    int64 base = mode == kIBSeekSet ? 0 : mode == kIBSeekCur ? (int64) pos_ : (int64) data_.size();
    pos_ = (size_t) std::max<int64>(0, std::min<int64>(base + pos, (int64) data_.size()));
    if (result) *result = (int64) pos_;
    return kResultOk;
  }
  tresult PLUGIN_API tell(int64* pos) override { if (pos) *pos = (int64) pos_; return kResultOk; }
 private:
  std::vector<char> data_;
  size_t pos_ = 0;
};

int main(int argc, char** argv) {
  if (argc < 2) { printf("usage: probe /abs/path/plugin.so [--params | --render OUT.f32]\n"); return 2; }
  bool list_params = argc > 2 && std::strcmp(argv[2], "--params") == 0;
  const char* render_path = argc > 3 && std::strcmp(argv[2], "--render") == 0 ? argv[3] : nullptr;
  const char* render_settings = render_path && argc > 4 ? argv[4] : nullptr;

  void* h = dlopen(argv[1], RTLD_NOW | RTLD_LOCAL);
  if (!h) { printf("dlopen FAILED: %s\n", dlerror()); return 1; }
  using Entry = bool (*)(void*);
  if (auto e = (Entry) dlsym(h, "ModuleEntry")) { if (!e(h)) { printf("ModuleEntry FAILED\n"); return 1; } }
  auto get = (IPluginFactory* (*)()) dlsym(h, "GetPluginFactory");
  if (!get) { printf("no GetPluginFactory\n"); return 1; }
  IPluginFactory* f = get();

  PFactoryInfo fi; f->getFactoryInfo(&fi);
  printf("vendor : %s\nurl    : %s\nclasses: %d\n", fi.vendor, fi.url, f->countClasses());
  TUID component_cid = {};
  for (int i = 0; i < f->countClasses(); ++i) {
    PClassInfo ci; f->getClassInfo(i, &ci);
    printf("  [%d] %-24s %-28s cid=", i, ci.name, ci.category);
    for (int b = 0; b < 16; ++b) printf("%02X", (unsigned char) ci.cid[b]);
    printf("\n");
    if (std::strcmp(ci.category, kVstAudioEffectClass) == 0)
      std::memcpy(component_cid, ci.cid, sizeof(TUID));
  }

  int status = 0;
  if (list_params || render_path) {
    IComponent* component = nullptr;
    if (f->createInstance(component_cid, IComponent::iid, (void**) &component) != kResultOk || !component) {
      printf("createInstance(IComponent) FAILED\n"); return 1;
    }
    component->initialize(nullptr);

    TUID controller_cid;
    IEditController* controller = nullptr;
    if (component->getControllerClassId(controller_cid) == kResultOk)
      f->createInstance(controller_cid, IEditController::iid, (void**) &controller);
    if (!controller)
      component->queryInterface(IEditController::iid, (void**) &controller);
    if (!controller) { printf("no IEditController\n"); return 1; }
    controller->initialize(nullptr);

    // Hosts connect the two halves before asking for parameters, and JUCE's
    // controller only learns them through this connection. Without it the
    // count is 0 - which the first version of this probe reported.
    IConnectionPoint* component_cp = nullptr;
    IConnectionPoint* controller_cp = nullptr;
    component->queryInterface(IConnectionPoint::iid, (void**) &component_cp);
    controller->queryInterface(IConnectionPoint::iid, (void**) &controller_cp);
    if (component_cp && controller_cp) {
      component_cp->connect(controller_cp);
      controller_cp->connect(component_cp);
    }

    if (const char* state_path = std::getenv("GNARL_PROBE_STATE")) {
      std::ifstream in(state_path, std::ios::binary);
      MemoryStream stream(std::vector<char>((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>()));
      printf("setState(%s): %s\n", state_path, component->setState(&stream) == kResultOk ? "ok" : "REFUSED");
    }
    if (render_path) {
      status = render(component, render_path, render_settings);
    }
    else {
      int count = controller->getParameterCount();
      printf("parameters: %d\n", count);
      for (int i = 0; i < count; ++i) {
        ParameterInfo info;
        controller->getParameterInfo(i, info);
        printf("  %4d  id=%-10u %s\n", i, (unsigned) info.id, utf8(info.title).c_str());
      }
    }
    if (component_cp && controller_cp) {
      component_cp->disconnect(controller_cp);
      controller_cp->disconnect(component_cp);
    }
    if (component_cp) component_cp->release();
    if (controller_cp) controller_cp->release();
    controller->terminate();
    controller->release();
    component->terminate();
    component->release();
  }

  f->release();
  if (auto x = (bool (*)()) dlsym(h, "ModuleExit")) x();
  return status;
}
