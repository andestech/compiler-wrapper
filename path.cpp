#include <limits.h>
#include <unistd.h>

#include <string>
#ifdef __APPLE__
#include <mach-o/dyld.h>
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
#elif defined(__MINGW32__)
  SmallVector<wchar_t, MAX_PATH> PathName;
  PathName.resize_for_overwrite(PathName.capacity());
  DWORD Size = ::GetModuleFileNameW(NULL, PathName.data(), PathName.size());

  // A zero return value indicates a failure other than insufficient space.
  if (Size == 0)
    return "";

  // Insufficient space is determined by a return value equal to the size of
  // the buffer passed in.
  if (Size == PathName.capacity())
    return "";

  // On success, GetModuleFileNameW returns the number of characters written to
  // the buffer not including the NULL terminator.
  PathName.truncate(Size);

  // Convert the result from UTF-16 to UTF-8.
  SmallVector<char, MAX_PATH> PathNameUTF8;
  if (UTF16ToUTF8(PathName.data(), PathName.size(), PathNameUTF8))
    return "";

  //llvm::sys::path::make_preferred(PathNameUTF8);
  std::replace(PathNameUTF8.begin(), PathNameUTF8.end(), '\\', '/');
  return std::string(PathNameUTF8.data());
#else
#error GetMainExecutable is not implemented on this host yet.
#endif
  return "";
}
