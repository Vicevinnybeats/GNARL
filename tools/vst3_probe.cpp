// tools/vst3_probe.cpp - loads a VST3 the way a host does and reports it.
//
//   g++ -std=c++17 -I third_party/VST_SDK/VST3_SDK tools/vst3_probe.cpp -o probe -ldl
//   ./probe /abs/path/GNARL.so            factory: vendor, classes, class IDs
//   ./probe /abs/path/GNARL.so --params   also instantiates the plugin and lists
//                                         every parameter in HOST order
//
// Compiling is not loading. A plugin can build and link and still fail here,
// which is the only step a DAW's scanner cares about - and the parameter
// order printed by --params is the order a DAW keys automation lanes by, so
// a parameter added anywhere but the END moves every lane after it.
#define INIT_CLASS_IID
#include <dlfcn.h>
#include <cstdio>
#include <cstring>
#include <string>
#include "pluginterfaces/base/ipluginbase.h"
#include "pluginterfaces/vst/ivstcomponent.h"
#include "pluginterfaces/vst/ivsteditcontroller.h"
#include "pluginterfaces/vst/ivstaudioprocessor.h"
#include "pluginterfaces/vst/ivstmessage.h"

using namespace Steinberg;
using namespace Steinberg::Vst;

static std::string utf8(const TChar* s) {
  std::string out;
  for (; *s; ++s) out += (*s < 128) ? static_cast<char>(*s) : '?';
  return out;
}

int main(int argc, char** argv) {
  if (argc < 2) { printf("usage: probe /abs/path/plugin.so [--params]\n"); return 2; }
  bool list_params = argc > 2 && std::strcmp(argv[2], "--params") == 0;

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
  if (list_params) {
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

    int count = controller->getParameterCount();
    printf("parameters: %d\n", count);
    for (int i = 0; i < count; ++i) {
      ParameterInfo info;
      controller->getParameterInfo(i, info);
      printf("  %4d  id=%-10u %s\n", i, (unsigned) info.id, utf8(info.title).c_str());
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
