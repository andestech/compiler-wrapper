#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>
#include <libgen.h>
#include <vector>
#include <string>
#include "config.h"

int main(int argc, const char * const argv[])
{
  char self_path[PATH_MAX];
  ssize_t len = readlink("/proc/self/exe", self_path, sizeof(self_path) - 1);
  if (len != -1) {
    self_path[len] = '\0';
  }

  char *dir = dirname(self_path);

  bool verbose = false;
  for (int i = 0; i < argc; ++i) {
    if ((strcmp (argv[i], "-v") == 0) ||
        (strcmp (argv[i], "--verbose") == 0)) {
      verbose = true;
    }
  }

  const char *prog;
  if (CLANGXX) {
    prog = "clang++";
  } else {
    prog = "clang";
  }

  std::vector<std::string> extra_arg_vec;

  extra_arg_vec.push_back("-Wno-unused-command-line-argument");

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
    extra_arg_vec.push_back("-fno-math-errno");
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

  if (strlen(REL_SYSROOT)) {
    std::string sysroot = dir;
    sysroot += "/";
    sysroot += REL_SYSROOT;
    extra_arg_vec.push_back("--sysroot=" + sysroot);
  }

  int new_argc = argc + extra_arg_vec.size();
  const char **new_args = new const char *[new_argc + 1];
  char *clang_path = new char[strlen(self_path) + strlen(prog) + 2];

  strcpy (clang_path, self_path);
  strcat (clang_path, "/");
  strcat (clang_path, prog);

  new_args[0] = clang_path;

  int sz = extra_arg_vec.size();
  for (int i = 0; i < sz; ++i) {
    new_args[i + 1] = extra_arg_vec[i].c_str();
  }

  for (int i = 1; i < argc; ++i) {
    new_args[i + sz] = argv[i];
  }

  new_args[new_argc] = NULL;

#ifdef DEBUG
  printf ("self_path=\"%s\" %s clang_path=%s\n", dir, self_path, clang_path);
#endif

  if (verbose) {
    for (int i=0;i<new_argc;i++) {
      printf ("\"%s\" ", new_args[i]);
    }
    printf("\n");
  }

  fflush(stdout);

  int rv = execvp (clang_path, (char* const*)new_args);

  if (rv < 0) {
    fprintf (stderr, "%s not found %s\n", prog, clang_path);
  }

  return rv;
}
