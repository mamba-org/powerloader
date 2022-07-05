
#ifndef POWERLOADER_API_HPP
#define POWERLOADER_API_HPP

#ifdef POWERLOADER_STATIC_DEFINE
#  define POWERLOADER_API
#  define POWERLOADER_NO_EXPORT
#else
#  ifndef POWERLOADER_API
#    ifdef libpowerloader_EXPORTS
        /* We are building this library */
#      define POWERLOADER_API __declspec(dllexport)
#    else
        /* We are using this library */
#      define POWERLOADER_API __declspec(dllimport)
#    endif
#  endif

#  ifndef POWERLOADER_NO_EXPORT
#    define POWERLOADER_NO_EXPORT 
#  endif
#endif

#ifndef POWERLOADER_DEPRECATED
#  define POWERLOADER_DEPRECATED __declspec(deprecated)
#endif

#ifndef POWERLOADER_DEPRECATED_EXPORT
#  define POWERLOADER_DEPRECATED_EXPORT POWERLOADER_API POWERLOADER_DEPRECATED
#endif

#ifndef POWERLOADER_DEPRECATED_NO_EXPORT
#  define POWERLOADER_DEPRECATED_NO_EXPORT POWERLOADER_NO_EXPORT POWERLOADER_DEPRECATED
#endif

#if 0 /* DEFINE_NO_DEPRECATED */
#  ifndef POWERLOADER_NO_DEPRECATED
#    define POWERLOADER_NO_DEPRECATED
#  endif
#endif

#endif /* POWERLOADER_API_HPP */
