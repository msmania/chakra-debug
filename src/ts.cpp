#include <vector>
#include <sstream>
#include <windows.h>
#define KDEXT_64BIT
#include <wdbgexts.h>
#include "common.h"

void debug_object::dump(std::ostream &s) const {
  address_string s1(base_);
  s << s1 << std::endl;
}

namespace Js {
  class ScriptContext : public debug_object {
    address_t recycler{};

  public:
    virtual void load(address_t addr) {
      base_ = addr;
      recycler = load_pointer(
        addr + get_field_offset("Js::ScriptContext", "recycler"));
    }

    virtual void dump(std::ostream &s) const {
      address_string s1(base_),
                     s2(recycler);
      s << s1 << " Memory::Recycler " << s2;
    }
  };

  class JavascriptLibrary : public debug_object {
    address_t scriptContext{};
    address_t jsrtContextObject{};

  public:
    virtual void load(address_t addr) {
      base_ = addr;
      scriptContext = load_pointer(
        addr + get_field_offset("Js::JavascriptLibrary", "scriptContext"));
      jsrtContextObject = load_pointer(
        addr + get_field_offset("Js::JavascriptLibrary", "jsrtContextObject"));
    }

    virtual void dump(std::ostream &s) const {
      address_string s1(base_);
      s << s1 << ' ';
      Js::ScriptContext context;
      context.load(scriptContext);
      context.dump(s);
    }

    address_t GetJsrtContext() const {
      return jsrtContextObject;
    }
  };
}

class JsrtContext : public debug_object {
  address_t javascriptLibrary{};
  address_t runtime{};
  address_t previous{};
  address_t next{};

public:
  virtual void load(address_t addr) {
    base_ = addr;
    javascriptLibrary = load_pointer(
      addr + get_field_offset("JsrtContext", "javascriptLibrary"));
    runtime = load_pointer(addr + get_field_offset("JsrtContext", "runtime"));
    previous = load_pointer(addr + get_field_offset("JsrtContext", "previous"));
    next = load_pointer(addr + get_field_offset("JsrtContext", "next"));
  }

  virtual void dump(std::ostream &s) const {
    address_string s1(base_);
    s << s1 << ' ';
    Js::JavascriptLibrary library;
    library.load(javascriptLibrary);
    library.dump(s);
  }

  address_t GetRuntime() const {
    return runtime;
  }

  address_t Next() const {
    return next;
  }

};

class JsrtRuntime : public debug_object {
  address_t contextList{};

public:
  virtual void load(address_t addr) {
    base_ = addr;
    contextList = load_pointer(addr + get_field_offset("JsrtRuntime", "contextList"));
  }

  virtual void dump(std::ostream &s) const {
    address_string s1(base_);
    s << "JsrtRuntime " << s1 << std::endl;
    for (address_t addr = contextList; addr; ) {
      JsrtContext context;
      context.load(addr);
      s << ' ';
      context.dump(s);
      s << std::endl;
      addr = context.Next() & -4;
    }
  }
};

DECLARE_API(ts) {
  const auto vargs = get_args(args);
  if (vargs.size() < 2) return;

  auto addr = GetExpression(vargs[1].c_str());
  if (vargs[0] == "-l") {
    Js::JavascriptLibrary library;
    library.load(addr);
    addr = library.GetJsrtContext();
    goto JsrtContext;
  }
  else if (vargs[0] == "-c") {
  JsrtContext:
    JsrtContext context;
    context.load(addr);
    addr = context.GetRuntime();
    goto JsrtRuntime;
  }
  else if (vargs[0] == "-r") {
  JsrtRuntime:
    JsrtRuntime rt;
    rt.load(addr);
    std::stringstream ss;
    rt.dump(ss);
    dprintf("%s\n", ss.str().c_str());
  }
}
