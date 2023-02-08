// Copyright (c) 2021, QuantStack and Mamba Contributors
//
// Distributed under the terms of the BSD 3-Clause License.
//
// The full license is in the file LICENSE, distributed with this software.

#ifndef POWERLOADER_API_HPP
#define POWERLOADER_API_HPP

/*
// clang-format off
#ifdef POWERLOADER_STATIC
// As a static library: no symbol import/export.
#  define POWERLOADER_API
#else
// As a shared library: export symbols on build, import symbols on use.
#  ifdef POWERLOADER_EXPORTS
     // We are building this library
#    ifdef _MSC_VER
#         define POWERLOADER_API __declspec(dllexport)
#    else
#         define POWERLOADER_API __attribute__((__visibility__("default")))
#    endif
#  else
     // We are using this library
#    ifdef _MSC_VER
#         define POWERLOADER_API __declspec(dllimport)
#    else
#         define POWERLOADER_API // Symbol import is implicit on non-msvc compilers.
#    endif
#  endif
#endif
// clang-format on*/

// Project version
#define POWERLOADER_VERSION_MAJOR 0
#define POWERLOADER_VERSION_MINOR 5
#define POWERLOADER_VERSION_PATCH 1

// Binary version
#define POWERLOADER_BINARY_CURRENT 0
#define POWERLOADER_BINARY_REVISION 0
#define POWERLOADER_BINARY_AGE 1

#endif
