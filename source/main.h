#pragma once

#include <string>
#include <vector>
#include <sys/types.h>
#include <functional>
#include <mutex>
#include "SDL_ttf.h"
#include "Project.h"
#include "types.h"

typedef std::function<void(void)> GenericTask;

// save general gui and tool settings to appdata
void SaveSettings();
void QueueTask(const GenericTask &func);
