#pragma once
#include "Common/Win32.h"
#include <cstdint>
#include <optional>
#include <string>
namespace nexus {
// Windows' built-in, on-device text recognition (Windows.Media.Ocr) in the languages of the
// user's profile. Blocking: call from a worker thread. Pixels are 32-bit BGRA, top-down.
// Returns nullopt when recognition is unavailable; an empty string when no text was found.
struct OcrText {std::wstring text,language;};
std::optional<OcrText> recognizeText(const uint8_t* bgra,int width,int height);
}
