#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>
#include <libgen.h>
#include <vector>
#include <string>
#include "config.h"

std::string getMainExecutableImpl(const char *argv0, void *MainAddr);

int main(int argc, const char * const argv[])
{
  char self_path[PATH_MAX];

  std::string exe_path = getMainExecutableImpl(nullptr, nullptr);

  strncpy(self_path, exe_path.c_str(), PATH_MAX);

  /* From man page,
   * Both dirname() and basename() may modify the contents of path, so it may
   * be desirable to pass a copy when calling one of these functions.  */
  char *dir = dirname(self_path);

  bool verbose = false;
  for (int i = 0; i < argc; ++i) {
    if ((strcmp (argv[i], "-v") == 0) ||
        (strcmp (argv[i], "--verbose") == 0)) {
      verbose = true;
    }
  }

  std::vector<std::string> extra_arg_vec;
  if (strcmp(LIBC, "mculib") == 0) {
    extra_arg_vec.push_back("-fno-math-errno");
  }

#ifdef CLANGXX
  extra_arg_vec.push_back("-Wno-unused-command-line-argument");
  extra_arg_vec.push_back("-ffinite-loops");

  if (strlen(ARCH)) {
    extra_arg_vec.push_back("-march=" ARCH);
  }

  if (strlen(ABI)) {
    extra_arg_vec.push_back("-mabi=" ABI);
  }

  if (strlen(TARGET)) {
    extra_arg_vec.push_back("--target=" TARGET);
  }

  if (strlen(LIBC)) {
    extra_arg_vec.push_back("-mlibc=" LIBC);
  }

  if (strcmp(LIBC, "mculib") == 0) {
    extra_arg_vec.push_back("-mriscv-iprintf");
  }

  if (!strcmp(LIBC, "mculib") || !strcmp(LIBC, "newlib")  ) {
    extra_arg_vec.push_back("-fno-delete-null-pointer-checks");
  }

  if (strlen(LD)) {
    extra_arg_vec.push_back("-fuse-ld=" LD);
  }

  if (strlen(MULTI_LIB_LIST)) {
    extra_arg_vec.push_back("--with-multi-lib=" MULTI_LIB_LIST);
  }

  if (strlen(EXTRA_FLAGS)) {
    extra_arg_vec.push_back(EXTRA_FLAGS);
  }

  extra_arg_vec.push_back("-ffp-contract=fast");

  if (strlen(REL_SYSROOT)) {
    std::string sysroot = dir;
    sysroot += "/";
    sysroot += REL_SYSROOT;
    extra_arg_vec.push_back("--sysroot=" + sysroot);
  }
#endif


  const char *prog;
#ifdef CLANGXX
  if (CLANGXX) {
    prog = "clang++";
  } else {
    prog = "clang";
  }
#endif
#ifdef GXX
  if (GXX) {
    prog = TARGET "-g++.gnu";
  } else {
    prog = TARGET "-gcc.gnu";
  }
#endif

  int new_argc = argc + extra_arg_vec.size();
  const char **new_args = new const char *[new_argc + 1];
  char *cc_path = new char[strlen(self_path) + strlen(prog) + 2];

  strcpy (cc_path, dir);
  strcat (cc_path, "/");
  strcat (cc_path, prog);

  new_args[0] = cc_path;

  int sz = extra_arg_vec.size();
  for (int i = 0; i < sz; ++i) {
    new_args[i + 1] = extra_arg_vec[i].c_str();
  }

  for (int i = 1; i < argc; ++i) {
    new_args[i + sz] = argv[i];
  }

  new_args[new_argc] = NULL;

#ifdef DEBUG
  printf ("self_path=\"%s\" %s cc_path=%s\n", dir, self_path, cc_path);
#endif

  if (verbose) {
    for (int i = 0; i < new_argc; i++) {
      printf ("\"%s\" ", new_args[i]);
    }
    printf("\n");
  }

  fflush(stdout);

  int rv = execvp (cc_path, (char* const*)new_args);

  if (rv < 0) {
    fprintf (stderr, "%s not found %s\n", prog, cc_path);
  }

  return rv;
}
