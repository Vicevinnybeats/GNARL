// tools/vst3_probe.cpp - build: g++ -std=c++17 -I third_party/VST_SDK/VST3_SDK tools/vst3_probe.cpp -ldl
// Loads a VST3 the way a host does: dlopen, ModuleEntry, GetPluginFactory,
// then asks the factory what it is. A plugin can compile and link and still
// fail here, which is the only step a DAW's scanner actually cares about.
#include <dlfcn.h>
#include <cstdio>
#include "pluginterfaces/base/ipluginbase.h"
using namespace Steinberg;
int main(int argc, char** argv) {
  void* h = dlopen(argv[1], RTLD_NOW | RTLD_LOCAL);
  if (!h) { printf("dlopen FAILED: %s\n", dlerror()); return 1; }
  using Entry = bool (*)(void*);
  if (auto e = (Entry) dlsym(h, "ModuleEntry")) { if (!e(h)) { printf("ModuleEntry FAILED\n"); return 1; } }
  auto get = (IPluginFactory* (*)()) dlsym(h, "GetPluginFactory");
  if (!get) { printf("no GetPluginFactory\n"); return 1; }
  IPluginFactory* f = get();
  PFactoryInfo fi; f->getFactoryInfo(&fi);
  printf("vendor : %s\nurl    : %s\nclasses: %d\n", fi.vendor, fi.url, f->countClasses());
  for (int i = 0; i < f->countClasses(); ++i) {
    PClassInfo ci; f->getClassInfo(i, &ci);
    printf("  [%d] %-24s %-28s cid=", i, ci.name, ci.category);
    for (int b = 0; b < 16; ++b) printf("%02X", (unsigned char) ci.cid[b]);
    printf("\n");
  }
  f->release();
  if (auto x = (bool (*)()) dlsym(h, "ModuleExit")) x();
  return 0;
}
