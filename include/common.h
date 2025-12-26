#pragma once

#define DBG_MACRO_NO_WARNING
#include "dbg.h"

#include <cstdint>
#include <limits>
#include <filesystem>
#include <fstream>
#include <string>
#include <cstring>
#include <cassert>
#include <deque>
#include <queue>
#include <vector>
#include <array>
#include <cmath>

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

using u8  = std::uint8_t;
using u16 = std::uint16_t;
using u32 = std::uint32_t;
using u64 = std::uint64_t;

using i8  = std::int8_t;
using i16 = std::int16_t;
using i32 = std::int32_t;
using i64 = std::int64_t;