#include <windows.h>
#define KDEXT_64BIT
#include <wdbgexts.h>

// https://msdn.microsoft.com/en-us/library/windows/hardware/ff543968(v=vs.85).aspx
static EXT_API_VERSION ApiVersion = {
  0, 0,                       // These values are not checked by debugger. Any value is ok.
  EXT_API_VERSION_NUMBER64,   // Must be 64bit to get `args` pointer
  0                           // Reserved
};

WINDBG_EXTENSION_APIS ExtensionApis;

BOOL WINAPI DllMain(HINSTANCE, DWORD, LPVOID) {
  return TRUE;
}

LPEXT_API_VERSION ExtensionApiVersion(void) {
  return &ApiVersion;
}

VOID WinDbgExtensionDllInit(PWINDBG_EXTENSION_APIS lpExtensionApis,
                            USHORT MajorVersion,
                            USHORT MinorVersion) {
  ExtensionApis = *lpExtensionApis;
  return;
}

DECLARE_API(help) {
dprintf(
  "\n# Recycler\n"
  "!ts [-r JsrtRuntime] [-c JsrtContext] [-l Js::JavascriptLibrary]\n"
  "!heap [-r Memory::Recycler] [-m Memory::HeapInfoManager]\n"
  );
}
