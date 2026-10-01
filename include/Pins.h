#pragma once

// Select exactly one hardware layout; never silently fall back to another board.
#if defined(RACESYNC_BOARD_DEVKIT_S3) == defined(RACESYNC_BOARD_XIAO_S3_PLUS)
#error "Select exactly one RaceSync board in PlatformIO"
#elif defined(RACESYNC_BOARD_DEVKIT_S3)
#include "boards/DevKitS3.h"
#else
#include "boards/XiaoS3Plus.h"
#endif
