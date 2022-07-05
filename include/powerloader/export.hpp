
#ifndef POWERLOADER_API_HPP
#define POWERLOADER_API_HPP


#ifdef POWERLOADER_STATIC
// As a static library: no symbol import/export.
#  define POWERLOADER_API
#else
 // As a shared library: export symbols on build, import symbols on use.
#  ifdef POWERLOADER_EXPORTS
     // We are building this library
#    define POWERLOADER_API __declspec(dllexport)
#  else
     // We are using this library
#    define POWERLOADER_API __declspec(dllimport)
#  endif
#endif

#endif
