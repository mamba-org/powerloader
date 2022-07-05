
#ifndef POWERLOADER_API_HPP
#define POWERLOADER_API_HPP


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

#endif
