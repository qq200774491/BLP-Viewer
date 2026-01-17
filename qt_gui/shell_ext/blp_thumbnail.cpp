#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <windows.h>
#include <ShlObj.h>
#include <thumbcache.h>
#include <shlwapi.h>
#include <objidl.h>

#include <algorithm>
#include <new>
#include <vector>

#include "blp_lib.h"

#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "shlwapi.lib")

namespace {

const CLSID CLSID_BlpThumbnailProvider = {
    0x27A35239,
    0x0B87,
    0x4085,
    {0x89, 0x44, 0x46, 0x3B, 0x44, 0x0D, 0x16, 0x2F}};

const wchar_t* kThumbnailClsid = L"{27A35239-0B87-4085-8944-463B440D162F}";
const wchar_t* kThumbnailHandler = L"{E357FCCD-A995-4576-B01F-234630154E96}";
const wchar_t* kBlpProgId = L"BLPViewer.File";

HINSTANCE g_hInstance = nullptr;
long g_cDllRef = 0;

typedef BlpResult (*blp_load_from_buffer_fn)(const uint8_t*, uint32_t, BlpImage*);
typedef void (*blp_free_image_fn)(BlpImage*);

HMODULE g_blpModule = nullptr;
blp_load_from_buffer_fn g_blpLoad = nullptr;
blp_free_image_fn g_blpFree = nullptr;

bool ensureBlpLoaded() {
    if (g_blpModule && g_blpLoad && g_blpFree) {
        return true;
    }

    wchar_t envPath[MAX_PATH] = {};
    DWORD envLen = GetEnvironmentVariableW(L"BLP_LIB_PATH", envPath, MAX_PATH);
    if (envLen > 0 && envLen < MAX_PATH) {
        g_blpModule = LoadLibraryW(envPath);
    }

    if (!g_blpModule) {
        wchar_t modulePath[MAX_PATH] = {};
        if (GetModuleFileNameW(g_hInstance, modulePath, MAX_PATH) > 0) {
            PathRemoveFileSpecW(modulePath);
            const wchar_t* candidates[] = {L"blp_lib.dll", L"blp-windows.dll"};
            for (const wchar_t* name : candidates) {
                wchar_t fullPath[MAX_PATH] = {};
                if (PathCombineW(fullPath, modulePath, name)) {
                    g_blpModule = LoadLibraryW(fullPath);
                    if (g_blpModule) {
                        break;
                    }
                }
            }
        }
    }

    if (!g_blpModule) {
        g_blpModule = LoadLibraryW(L"blp_lib.dll");
    }
    if (!g_blpModule) {
        g_blpModule = LoadLibraryW(L"blp-windows.dll");
    }

    if (!g_blpModule) {
        return false;
    }

    g_blpLoad = reinterpret_cast<blp_load_from_buffer_fn>(
        GetProcAddress(g_blpModule, "blp_load_from_buffer"));
    g_blpFree = reinterpret_cast<blp_free_image_fn>(
        GetProcAddress(g_blpModule, "blp_free_image"));

    return g_blpLoad && g_blpFree;
}

HRESULT readStream(IStream* stream, std::vector<uint8_t>& outBytes) {
    if (!stream) {
        return E_INVALIDARG;
    }

    STATSTG stat = {};
    HRESULT hr = stream->Stat(&stat, STATFLAG_NONAME);
    if (FAILED(hr)) {
        return hr;
    }
    if (stat.cbSize.HighPart != 0) {
        return E_FAIL;
    }

    const ULONG size = stat.cbSize.LowPart;
    if (size == 0) {
        return E_FAIL;
    }

    LARGE_INTEGER zero = {};
    stream->Seek(zero, STREAM_SEEK_SET, nullptr);

    outBytes.resize(size);
    ULONG read = 0;
    hr = stream->Read(outBytes.data(), size, &read);
    if (FAILED(hr) || read != size) {
        return E_FAIL;
    }
    return S_OK;
}

std::vector<uint8_t> rgbaToBgra(const uint8_t* rgba, size_t pixelCount) {
    std::vector<uint8_t> bgra(pixelCount * 4);
    for (size_t i = 0; i < pixelCount; ++i) {
        const size_t idx = i * 4;
        bgra[idx + 0] = rgba[idx + 2];
        bgra[idx + 1] = rgba[idx + 1];
        bgra[idx + 2] = rgba[idx + 0];
        bgra[idx + 3] = rgba[idx + 3];
    }
    return bgra;
}

HBITMAP createThumbnailBitmap(const BlpImage& image, UINT cx) {
    if (image.width == 0 || image.height == 0 || !image.data) {
        return nullptr;
    }

    const double scale =
        std::min(static_cast<double>(cx) / image.width, static_cast<double>(cx) / image.height);
    const int destW = std::max(1, static_cast<int>(image.width * scale));
    const int destH = std::max(1, static_cast<int>(image.height * scale));

    BITMAPINFO destInfo = {};
    destInfo.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    destInfo.bmiHeader.biWidth = destW;
    destInfo.bmiHeader.biHeight = -destH;
    destInfo.bmiHeader.biPlanes = 1;
    destInfo.bmiHeader.biBitCount = 32;
    destInfo.bmiHeader.biCompression = BI_RGB;

    void* destBits = nullptr;
    HBITMAP hbmp = CreateDIBSection(nullptr, &destInfo, DIB_RGB_COLORS, &destBits, nullptr, 0);
    if (!hbmp) {
        return nullptr;
    }

    const size_t pixelCount = static_cast<size_t>(image.width) * image.height;
    const std::vector<uint8_t> bgra = rgbaToBgra(image.data, pixelCount);

    BITMAPINFO srcInfo = {};
    srcInfo.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    srcInfo.bmiHeader.biWidth = static_cast<int>(image.width);
    srcInfo.bmiHeader.biHeight = -static_cast<int>(image.height);
    srcInfo.bmiHeader.biPlanes = 1;
    srcInfo.bmiHeader.biBitCount = 32;
    srcInfo.bmiHeader.biCompression = BI_RGB;

    HDC hdc = CreateCompatibleDC(nullptr);
    HGDIOBJ old = SelectObject(hdc, hbmp);
    SetStretchBltMode(hdc, HALFTONE);
    POINT oldOrg = {};
    SetBrushOrgEx(hdc, 0, 0, &oldOrg);
    StretchDIBits(hdc,
                  0,
                  0,
                  destW,
                  destH,
                  0,
                  0,
                  static_cast<int>(image.width),
                  static_cast<int>(image.height),
                  bgra.data(),
                  &srcInfo,
                  DIB_RGB_COLORS,
                  SRCCOPY);
    SelectObject(hdc, old);
    DeleteDC(hdc);

    return hbmp;
}

class BlpThumbnailProvider : public IInitializeWithStream, public IThumbnailProvider {
public:
    BlpThumbnailProvider() : ref_(1) {
        InterlockedIncrement(&g_cDllRef);
    }

    ~BlpThumbnailProvider() {
        if (stream_) {
            stream_->Release();
        }
        InterlockedDecrement(&g_cDllRef);
    }

    IFACEMETHODIMP QueryInterface(REFIID riid, void** ppv) override {
        if (!ppv) {
            return E_POINTER;
        }
        *ppv = nullptr;
        if (IsEqualIID(riid, IID_IUnknown) || IsEqualIID(riid, IID_IInitializeWithStream)) {
            *ppv = static_cast<IInitializeWithStream*>(this);
        } else if (IsEqualIID(riid, IID_IThumbnailProvider)) {
            *ppv = static_cast<IThumbnailProvider*>(this);
        } else {
            return E_NOINTERFACE;
        }
        AddRef();
        return S_OK;
    }

    IFACEMETHODIMP_(ULONG) AddRef() override {
        return static_cast<ULONG>(InterlockedIncrement(&ref_));
    }

    IFACEMETHODIMP_(ULONG) Release() override {
        const long value = InterlockedDecrement(&ref_);
        if (value == 0) {
            delete this;
        }
        return static_cast<ULONG>(value);
    }

    IFACEMETHODIMP Initialize(IStream* stream, DWORD) override {
        if (!stream) {
            return E_INVALIDARG;
        }
        if (stream_) {
            return HRESULT_FROM_WIN32(ERROR_ALREADY_INITIALIZED);
        }
        stream_ = stream;
        stream_->AddRef();
        return S_OK;
    }

    IFACEMETHODIMP GetThumbnail(UINT cx, HBITMAP* phbmp, WTS_ALPHATYPE* pdwAlpha) override {
        if (!phbmp || !pdwAlpha) {
            return E_POINTER;
        }
        *phbmp = nullptr;
        *pdwAlpha = WTSAT_UNKNOWN;

        if (!ensureBlpLoaded()) {
            return E_FAIL;
        }

        std::vector<uint8_t> bytes;
        HRESULT hr = readStream(stream_, bytes);
        if (FAILED(hr)) {
            return hr;
        }

        BlpImage image = {};
        if (g_blpLoad(bytes.data(), static_cast<uint32_t>(bytes.size()), &image) != BLP_SUCCESS) {
            return E_FAIL;
        }

        HBITMAP hbmp = createThumbnailBitmap(image, cx);
        g_blpFree(&image);
        if (!hbmp) {
            return E_FAIL;
        }

        *phbmp = hbmp;
        *pdwAlpha = WTSAT_ARGB;
        return S_OK;
    }

private:
    long ref_;
    IStream* stream_ = nullptr;
};

class BlpClassFactory : public IClassFactory {
public:
    BlpClassFactory() : ref_(1) {
        InterlockedIncrement(&g_cDllRef);
    }

    ~BlpClassFactory() {
        InterlockedDecrement(&g_cDllRef);
    }

    IFACEMETHODIMP QueryInterface(REFIID riid, void** ppv) override {
        if (!ppv) {
            return E_POINTER;
        }
        *ppv = nullptr;
        if (IsEqualIID(riid, IID_IUnknown) || IsEqualIID(riid, IID_IClassFactory)) {
            *ppv = static_cast<IClassFactory*>(this);
            AddRef();
            return S_OK;
        }
        return E_NOINTERFACE;
    }

    IFACEMETHODIMP_(ULONG) AddRef() override {
        return static_cast<ULONG>(InterlockedIncrement(&ref_));
    }

    IFACEMETHODIMP_(ULONG) Release() override {
        const long value = InterlockedDecrement(&ref_);
        if (value == 0) {
            delete this;
        }
        return static_cast<ULONG>(value);
    }

    IFACEMETHODIMP CreateInstance(IUnknown* pUnkOuter, REFIID riid, void** ppv) override {
        if (pUnkOuter) {
            return CLASS_E_NOAGGREGATION;
        }
        auto* provider = new (std::nothrow) BlpThumbnailProvider();
        if (!provider) {
            return E_OUTOFMEMORY;
        }
        HRESULT hr = provider->QueryInterface(riid, ppv);
        provider->Release();
        return hr;
    }

    IFACEMETHODIMP LockServer(BOOL lock) override {
        if (lock) {
            InterlockedIncrement(&g_cDllRef);
        } else {
            InterlockedDecrement(&g_cDllRef);
        }
        return S_OK;
    }

private:
    long ref_;
};

bool setRegistryString(HKEY root, const wchar_t* subKey, const wchar_t* name, const wchar_t* value) {
    HKEY key = nullptr;
    if (RegCreateKeyExW(root, subKey, 0, nullptr, 0, KEY_SET_VALUE, nullptr, &key, nullptr) !=
        ERROR_SUCCESS) {
        return false;
    }
    const DWORD size = static_cast<DWORD>((wcslen(value) + 1) * sizeof(wchar_t));
    const LONG result =
        RegSetValueExW(key, name, 0, REG_SZ, reinterpret_cast<const BYTE*>(value), size);
    RegCloseKey(key);
    return result == ERROR_SUCCESS;
}

} // namespace

extern "C" BOOL APIENTRY DllMain(HMODULE hModule, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        g_hInstance = hModule;
        DisableThreadLibraryCalls(hModule);
    }
    return TRUE;
}

STDAPI DllCanUnloadNow(void) {
    return (g_cDllRef == 0) ? S_OK : S_FALSE;
}

STDAPI DllGetClassObject(REFCLSID rclsid, REFIID riid, void** ppv) {
    if (!IsEqualCLSID(rclsid, CLSID_BlpThumbnailProvider)) {
        return CLASS_E_CLASSNOTAVAILABLE;
    }

    auto* factory = new (std::nothrow) BlpClassFactory();
    if (!factory) {
        return E_OUTOFMEMORY;
    }
    HRESULT hr = factory->QueryInterface(riid, ppv);
    factory->Release();
    return hr;
}

STDAPI DllRegisterServer(void) {
    wchar_t modulePath[MAX_PATH] = {};
    if (!GetModuleFileNameW(g_hInstance, modulePath, MAX_PATH)) {
        return HRESULT_FROM_WIN32(GetLastError());
    }

    wchar_t clsidKey[MAX_PATH] = {};
    wsprintfW(clsidKey, L"Software\\Classes\\CLSID\\%s", kThumbnailClsid);
    if (!setRegistryString(HKEY_CURRENT_USER, clsidKey, nullptr, L"BLP Thumbnail Provider")) {
        return E_FAIL;
    }

    wchar_t inprocKey[MAX_PATH] = {};
    wsprintfW(inprocKey, L"%s\\InprocServer32", clsidKey);
    if (!setRegistryString(HKEY_CURRENT_USER, inprocKey, nullptr, modulePath)) {
        return E_FAIL;
    }
    if (!setRegistryString(HKEY_CURRENT_USER, inprocKey, L"ThreadingModel", L"Apartment")) {
        return E_FAIL;
    }

    wchar_t shellExKey[MAX_PATH] = {};
    wsprintfW(shellExKey, L"Software\\Classes\\.blp\\ShellEx\\%s", kThumbnailHandler);
    if (!setRegistryString(HKEY_CURRENT_USER, shellExKey, nullptr, kThumbnailClsid)) {
        return E_FAIL;
    }

    wchar_t sfaKey[MAX_PATH] = {};
    wsprintfW(sfaKey,
              L"Software\\Classes\\SystemFileAssociations\\.blp\\ShellEx\\%s",
              kThumbnailHandler);
    setRegistryString(HKEY_CURRENT_USER, sfaKey, nullptr, kThumbnailClsid);

    wchar_t progKey[MAX_PATH] = {};
    wsprintfW(progKey, L"Software\\Classes\\%s\\ShellEx\\%s", kBlpProgId, kThumbnailHandler);
    setRegistryString(HKEY_CURRENT_USER, progKey, nullptr, kThumbnailClsid);

    SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, nullptr, nullptr);
    return S_OK;
}

STDAPI DllUnregisterServer(void) {
    wchar_t clsidKey[MAX_PATH] = {};
    wsprintfW(clsidKey, L"Software\\Classes\\CLSID\\%s", kThumbnailClsid);
    SHDeleteKeyW(HKEY_CURRENT_USER, clsidKey);

    wchar_t shellExKey[MAX_PATH] = {};
    wsprintfW(shellExKey, L"Software\\Classes\\.blp\\ShellEx\\%s", kThumbnailHandler);
    SHDeleteKeyW(HKEY_CURRENT_USER, shellExKey);

    wchar_t sfaKey[MAX_PATH] = {};
    wsprintfW(sfaKey,
              L"Software\\Classes\\SystemFileAssociations\\.blp\\ShellEx\\%s",
              kThumbnailHandler);
    SHDeleteKeyW(HKEY_CURRENT_USER, sfaKey);

    wchar_t progKey[MAX_PATH] = {};
    wsprintfW(progKey, L"Software\\Classes\\%s\\ShellEx\\%s", kBlpProgId, kThumbnailHandler);
    SHDeleteKeyW(HKEY_CURRENT_USER, progKey);

    SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, nullptr, nullptr);
    return S_OK;
}
