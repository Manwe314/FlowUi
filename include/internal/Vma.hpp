#pragma once

// VMA is third-party code; suppress known Clang diagnostics that can flood
// consumer builds when strict warning levels are enabled.
#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wnullability-completeness"
#pragma clang diagnostic ignored "-Wnullability-extension"
#endif

#include "vk_mem_alloc.h"

#if defined(__clang__)
#pragma clang diagnostic pop
#endif
