// Blaze.hpp : Include file for standard system include files,
// or project specific include files.

#define USE_OPTICK (0)
#define OPTICK_ENABLE_GPU (0)
#pragma once
// #define VALIDATION_LAYERS_ENABLED
#include <Version.hpp>

#include <string_view>

constexpr std::string_view PROJECT_NAME = "Blaze";

namespace blaze
{
void run();
}

// TODO: Reference additional headers your program requires here.
