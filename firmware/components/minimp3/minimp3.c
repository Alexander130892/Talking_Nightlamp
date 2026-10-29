/**
 * @file  minimp3.c
 * @brief Implementation unit for the single-header minimp3 decoder.
 *
 * minimp3 (https://github.com/lieff/minimp3) is CC0 / public domain and
 * vendored unmodified in include/minimp3.h. See README.md for the commit.
 */

#define MINIMP3_IMPLEMENTATION
#define MINIMP3_ONLY_MP3        /* drop MPEG layer I/II support */
#include "minimp3.h"
