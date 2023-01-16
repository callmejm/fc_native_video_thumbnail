#include "fc_native_video_thumbnail_plugin.h"

// This must be included before many other Windows headers.
#include <windows.h>

// For getPlatformVersion; remove unless needed for your plugin implementation.
#include <VersionHelpers.h>

#include <flutter/method_channel.h>
#include <flutter/plugin_registrar_windows.h>
#include <flutter/standard_method_codec.h>

#include <iostream>
#include <string>
#include <locale>
#include <codecvt>
#include <memory>
#include <sstream>
#include <shlwapi.h>
#include <wincodec.h>
#include <thumbcache.h>
#include <gdiplus.h>
#include <gdiplusimaging.h>
#include <wincodec.h>
#include <wingdi.h>
#include <atlimage.h>

namespace fc_native_video_thumbnail {

// https://github.com/flutter/plugins/blob/main/packages/camera/camera_windows/windows/camera_plugin.cpp
// Looks for |key| in |map|, returning the associated value if it is present, or
// a nullptr if not.
const flutter::EncodableValue* ValueOrNull(const flutter::EncodableMap& map, const char* key) {
    auto it = map.find(flutter::EncodableValue(key));
    if (it == map.end()) {
        return nullptr;
    }
    return &(it->second);
}

// Looks for |key| in |map|, returning the associated int64 value if it is
// present, or std::nullopt if not.
std::optional<int64_t> GetInt64ValueOrNull(const flutter::EncodableMap& map,
    const char* key) {
    auto value = ValueOrNull(map, key);
    if (!value) {
        return std::nullopt;
    }

    if (std::holds_alternative<int32_t>(*value)) {
        return static_cast<int64_t>(std::get<int32_t>(*value));
    }
    auto val64 = std::get_if<int64_t>(value);
    if (!val64) {
        return std::nullopt;
    }
    return *val64;
}

// Converts the given UTF-8 string to UTF-16.
std::wstring Utf16FromUtf8(const std::string& utf8_string) {
    if (utf8_string.empty()) {
        return std::wstring();
    }
    int target_length =
        ::MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, utf8_string.data(),
            static_cast<int>(utf8_string.length()), nullptr, 0);
    if (target_length == 0) {
        return std::wstring();
    }
    std::wstring utf16_string;
    utf16_string.resize(target_length);
    int converted_length =
        ::MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, utf8_string.data(),
            static_cast<int>(utf8_string.length()),
            utf16_string.data(), target_length);
    if (converted_length == 0) {
        return std::wstring();
    }
    return utf16_string;
}

int SaveThumbnail(PCWSTR srcFile, PCWSTR destFile, int size, REFGUID type) {
    IShellItem* pSI;
    HRESULT hr = SHCreateItemFromParsingName(srcFile, NULL, IID_IShellItem, (void**)&pSI);
    if (!SUCCEEDED(hr)) return 1;

    IThumbnailCache* pThumbCache;
    hr = CoCreateInstance(CLSID_LocalThumbnailCache,
        NULL,
        CLSCTX_INPROC_SERVER,
        IID_PPV_ARGS(&pThumbCache));

    if (!SUCCEEDED(hr)) return 1;
    ISharedBitmap* pSharedBitmap = NULL;
    hr = pThumbCache->GetThumbnail(pSI,
        size,
        WTS_EXTRACT,
        &pSharedBitmap,
        NULL,
        NULL);

    if (!SUCCEEDED(hr) || !pSharedBitmap) return 1;
    HBITMAP hBitmap;
    hr = pSharedBitmap->GetSharedBitmap(&hBitmap);
    if (!SUCCEEDED(hr) || !hBitmap) return 1;

    pThumbCache->Release();

    // Save the bitmap to a file
    CImage image;
    image.Attach(hBitmap);
    hr = image.Save(destFile, type);
    if (!SUCCEEDED(hr)) return 1;
    return 0;
}

// static
void FcNativeVideoThumbnailPlugin::RegisterWithRegistrar(
    flutter::PluginRegistrarWindows *registrar) {
  auto channel =
      std::make_unique<flutter::MethodChannel<flutter::EncodableValue>>(
          registrar->messenger(), "fc_native_video_thumbnail",
          &flutter::StandardMethodCodec::GetInstance());

  auto plugin = std::make_unique<FcNativeVideoThumbnailPlugin>();

  channel->SetMethodCallHandler(
      [plugin_pointer = plugin.get()](const auto &call, auto result) {
        plugin_pointer->HandleMethodCall(call, std::move(result));
      });

  registrar->AddPlugin(std::move(plugin));
}

FcNativeVideoThumbnailPlugin::FcNativeVideoThumbnailPlugin() {}

FcNativeVideoThumbnailPlugin::~FcNativeVideoThumbnailPlugin() {}

void FcNativeVideoThumbnailPlugin::HandleMethodCall(
    const flutter::MethodCall<flutter::EncodableValue> &method_call,
    std::unique_ptr<flutter::MethodResult<flutter::EncodableValue>> result) {
    const auto* argsPtr = std::get_if<flutter::EncodableMap>(method_call.arguments());
    assert(argsPtr);
    auto args = *argsPtr;
  if (method_call.method_name().compare("getThumbnailFile") == 0) {
      // Required arguments are enforced on dart side.
      const auto* src_file =
          std::get_if<std::string>(ValueOrNull(args, "srcFile"));
      assert(src_file);

      const auto* dest_file =
          std::get_if<std::string>(ValueOrNull(args, "destFile"));
      assert(dest_file);

      // NOTE: `width` is used as thumbnail size.
      const auto* width =
          std::get_if<int>(ValueOrNull(args, "width"));
      assert(width);

      const auto* outType =
          std::get_if<std::string>(ValueOrNull(args, "type"));
      assert(outType);

    auto save_res = SaveThumbnail(Utf16FromUtf8(*src_file).c_str(), Utf16FromUtf8(*dest_file).c_str(), *width, outType->compare("png") == 0 ? Gdiplus::ImageFormatPNG : Gdiplus::ImageFormatJPEG);

    if (save_res) {
      result->Error("Err", "Operation failed");
    } else {
        result->Success(flutter::EncodableValue(nullptr));
    }
  } else {
    result->NotImplemented();
  }
}

}  // namespace fc_native_video_thumbnail