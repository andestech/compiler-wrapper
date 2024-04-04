#include <limits.h>
#include <unistd.h>

#include <string>

#ifdef __APPLE__
#include <mach-o/dyld.h>
#endif

#if defined(_WIN32)
#include <windows.h>
#endif

#if defined(_WIN32)
static
int UTF16ToCodePage(unsigned codepage, const wchar_t *utf16,
                                size_t utf16_len,
                                char *converted) {
  if (utf16_len) {
    // Get length.
    int len = ::WideCharToMultiByte(codepage, 0, utf16, utf16_len, converted,
                                    0, NULL, NULL);

    if (len == 0) {
      return -1;
    }

    // Now do the actual conversion.
    len = ::WideCharToMultiByte(codepage, 0, utf16, utf16_len, converted,
                                MAX_PATH, NULL, NULL);

    if (len == 0) {
      return -1;
    }
    // Make the new string null terminated.
    converted[len] = '\0';
  }

  return 0;
}

int UTF16ToUTF8(const wchar_t *utf16, size_t utf16_len,
                            char *utf8) {
  return UTF16ToCodePage(CP_UTF8, utf16, utf16_len, utf8);
}
#endif

/// GetMainExecutable - Return the path to the main executable, given the
/// value of argv[0] from program startup.
std::string getMainExecutableImpl(const char *argv0, void *MainAddr) {
#if defined(__APPLE__)
  // On OS X the executable path is saved to the stack by dyld. Reading it
  // from there is much faster than calling dladdr, especially for large
  // binaries with symbols.
  char exe_path[PATH_MAX];
  uint32_t size = sizeof(exe_path);
  if (_NSGetExecutablePath(exe_path, &size) == 0) {
    char link_path[PATH_MAX];
    if (realpath(exe_path, link_path))
      return link_path;
  }
#elif defined(__linux__) || defined(__CYGWIN__) || defined(__gnu_hurd__)
  char exe_path[PATH_MAX];
  const char *aPath = "/proc/self/exe";
  //if (sys::fs::exists(aPath))
  {
    // /proc is not always mounted under Linux (chroot for example).
    ssize_t len = readlink(aPath, exe_path, sizeof(exe_path));
    if (len < 0)
      return "";

    // Null terminate the string for realpath. readlink never null
    // terminates its output.
    len = std::min(len, ssize_t(sizeof(exe_path) - 1));
    exe_path[len] = '\0';

    // On Linux, /proc/self/exe always looks through symlinks. However, on
    // GNU/Hurd, /proc/self/exe is a symlink to the path that was used to start
    // the program, and not the eventual binary file. Therefore, call realpath
    // so this behaves the same on all platforms.
#if _POSIX_VERSION >= 200112 || defined(__GLIBC__)
    if (char *real_path = realpath(exe_path, nullptr)) {
      std::string ret = std::string(real_path);
      free(real_path);
      return ret;
    }
#else
    char real_path[PATH_MAX];
    if (realpath(exe_path, real_path))
      return std::string(real_path);
#endif
  }
  // Fall back to the classical detection.
  //if (getprogpath(exe_path, argv0))
  //  return exe_path;
#elif defined(_WIN32)
  // The first argument may contain just the name of the executable (e.g.,
  // "clang") rather than the full path, so swap it with the full path.
  wchar_t ModuleName[MAX_PATH];
  size_t Length = ::GetModuleFileNameW(NULL, ModuleName, MAX_PATH);
  if (Length == 0 || Length == MAX_PATH) {
    return "";
  }

  // If the first argument is a shortened (8.3) name (which is possible even
  // if we got the module name), the driver will have trouble distinguishing it
  // (e.g., clang.exe v. clang++.exe), so expand it now.
  Length = GetLongPathNameW(ModuleName, ModuleName, MAX_PATH);
  if (Length == 0)
    return "";
  if (Length > MAX_PATH) {
    // We're not going to try to deal with paths longer than MAX_PATH, so we'll
    // treat this as an error.  GetLastError() returns ERROR_SUCCESS, which
    // isn't useful, so we'll hardcode an appropriate error value.
    return "";
  }

  char ModuleNameUTF8[MAX_PATH];
  int EC = UTF16ToUTF8(ModuleName, Length, ModuleNameUTF8);
  if (EC)
    return "";

  return std::string(ModuleNameUTF8);
#else
#error GetMainExecutable is not implemented on this host yet.
#endif
  return "";
}
