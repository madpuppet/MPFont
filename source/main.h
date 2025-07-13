#pragma once

#include <string>
#include <vector>
#include <sys/types.h>
#include <functional>
#include <mutex>
#include "SDL_ttf.h"

typedef uint64_t        u64;
typedef int64_t         i64;
typedef uint32_t        u32;
typedef int32_t         i32;
typedef uint16_t        u16;
typedef int16_t         i16;
typedef uint8_t         u8;
typedef int8_t          i8;
typedef float           f32;
typedef double          f64;

typedef std::function<void(void)> Callback;

struct Project
{
    std::string name;
    std::string ttf_name;
    TTF_Font* ttf_font = nullptr;
    std::vector<u16> chars;
    std::mutex internalMutex;
    bool open = true;
};

// add a callback to run on the main thread next update
void AddMainTask(const Callback& func);

