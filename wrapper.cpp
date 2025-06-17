#include "config.h"
#include <algorithm>
#include <iostream>
#include <regex>
#include <libgen.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>
#include <string>
#include <unistd.h>
#include <vector>

#ifdef _WIN32
/* Get libiberty declarations.  */
#define HAVE_DECL_BASENAME 1
#include "libiberty.h"
#include "gcc_system.h"
#endif

std::string getMainExecutableImpl(const char *argv0, void *MainAddr);

/* CPU names must match according entries in
 * riscv-cores.def/NDSRISCVProcessors.td for GCC/LLVM. */
static bool isAndes23Series(std::string const &cpu) {
  if (cpu == "d23")
    return true;
  return false;
}

static bool isAndes45Series(std::string const &cpu) {
  if (cpu == "a45" || cpu == "ax45" || cpu == "ax45mpv" || cpu == "n45" ||
      cpu == "n45f" || cpu == "nx45" || cpu == "nx45v" || cpu == "d45" ||
      cpu == "d45f" || cpu == "nx45f")
    return true;
  return false;
}

static bool isAndes46Series(std::string const &cpu) {
  if (cpu == "a46" || cpu == "a46mp" || cpu == "a46mpv" ||
      cpu == "ax46" || cpu == "ax46mp" || cpu == "ax46mpv")
    return true;
  return false;
}

static bool isAndes60Series(std::string const &cpu) {
  if (cpu == "ax60" || cpu == "ax65")
    return true;
  return false;
}

static bool isAndes66Series(std::string const &cpu) {
  if (cpu == "ax66")
    return true;
  return false;
}

/* Append -m* options to ARG_VEC based on CPU. */
static void append_cpu_options(std::string const &cpu,
                               std::vector<std::string> &arg_vec) {
  static const std::vector<std::string> andes_23_series = {
      "-mext-zc", "-mext-zbabcs", "-mext-cmo"};
  static const std::vector<std::string> andes_45_series = {
      "-mcmov", "-mext-zvlsseg"};
  static const std::vector<std::string> andes_46_series = {
      "-mext-zc", "-mext-zbabcs", "-mext-cmo", "-mext-svinval", "-mext-zvlsseg"};
  static const std::vector<std::string> andes_60_series = {
      "-mext-zbabcs",  "-mext-zkns", "-mext-cmo",
      "-mext-svinval", "-mcmov",     "-mno-execit"};
  static const std::vector<std::string> andes_66_series = {
      "-mext-cmo", "-mext-svinval", "-mno-execit"};

  auto add_options = [&arg_vec](const std::vector<std::string> &table) -> void {
    for (auto E : table)
      arg_vec.push_back(E);
  };

  if (isAndes23Series(cpu))
    add_options(andes_23_series);
  else if (isAndes45Series(cpu))
    add_options(andes_45_series);
  else if (isAndes46Series(cpu))
    add_options(andes_46_series);
  else if (isAndes60Series(cpu))
    add_options(andes_60_series);
  else if (isAndes66Series(cpu))
    add_options(andes_66_series);

  return;
}

/* Append -march=new_arch to ARG_VEC based on CPU. Return true if success. */
static bool append_cpu_march(std::string const &cpu,
                             std::vector<std::string> &arg_vec) {
  if (cpu.empty())
    return false;
  /* ARCH is the arch string defined in bs3/ToolConfig. It's treated as the
     default_arch and extensions placed before the first '_' are treated as
     the base_arch, i.e.,
         default_arch = ARCH = rv32imfdc_zicbom_zfa...
                               ^~~~~~~~~
                               base_arch
     The new_arch consists of the base_arch, cpu base & cpu addons, and the
     andes_generic_suffix, i.e.,
         new_arch =
           [base_arch]_[andes_cpu_base]_[andes_cpu_*_addon]+_[andes_generic_suffix]

     In general, only the base_arch part of the default_arch is unmodified
     and transfered to the new_arch.
     But there's one exception for Zc*:
         - If a new_arch contains Zca, it means that we want to override
           RVC/Zc* defined in default_arch. Thus the base_arch will be
           modified to remove RVC, and any Zc* in the default_arch will
           be neglected.
         - If a new_arch doesn't contains Zca but the default_arch contains
           Zc*, in additional to the unmodified base_arch, all Zc* will be
           picked from the default_arch to new_arch.
     E.g.,
         [default_arch]    +  [cpu_base]  =  [new_arch]
         rv32imfdc         +  zca         =  rv32imfd_zca
         rv32imfd_zca_zcf  +  zca         =  rv32imfd_zca
         rv32imfd_zca_zcf  +  zimop       =  rv32imfd_zimop_zca_zcf
  */
  static const std::string andes_generic_suffix = "_zicsr_zifencei_xandes";
  /* Andes cores.
     - Each entry of a table should start with a "_" unless it's a empty
       string.
     - Only define Zc* if a cpu need to override the RVC/Zc* settings
       in the default_arch.
     - Testcases are placed in exter-gcc-testsuite.
  */
  static const std::string andes_23_base =
    "_zicbop_zicbom_zicboz_zca_zcb_zcmp_zcmt_zba_zbb_zbc_zbs";
  static const std::vector<std::string> andes_23_float_addon = {
    "",     // v5
    "_zcf", // v5f
    "_zcf"  // v5d, do not use zcd since it conflicts with zcmp/zcmt
  };
  static const std::string andes_46_base = 
    "_zic64b_zicbom_zicbop_zicboz"
    "_ziccamoa_ziccif_zicclsm_ziccrse_zicntr"
    "_zihintpause_zihpm"
    "_zba_zbb_zbc_zbs"
    "_zca_zcb_zcmp_zcmt"
    "_ssccptr_sscounterenw_sstvala_sstvecd"
    "_svade_svbare_svinval_svpbmt";
  static const std::vector<std::string> andes_46_float_addon = {
    "",                     // v5
    "_zcf_zfbfmin_zfhmin",  // v5f
    "_zcf_zfbfmin_zfhmin"   // v5d, use zcmp/zcmt
  };
  static const std::vector<std::string> andes_46_atomic_addon = {
    "",                    // elf toolchain
    "_za64rs_zaamo_zalrsc" // linux toolchain
  };
  static const std::string andes_60_base = 
    "";
  static const std::vector<std::string> andes_60_float_addon = {
    "",  // v5
    "",  // v5f
    ""   // v5d
  };
  static const std::string andes_66_base =
    "_zic64b_zicbom_zicbop_zicboz_ziccamoa_ziccif_zicclsm_ziccrse_zicfilp"
    "_zicfiss_zicntr_zicond_zihintntl_zihintpause_zihpm_zimop_zba_zbb_zbc_zbs"
    "_zmmul_zca_zcb_zcmop"
    "_sha_shcounterenw_shgatpa_shtvala_shvsatpa_shvstvala_shvstvecd_ssccptr"
    "_sscofpmf_sscounterenw_ssnpm_sspm_ssstateen_ssstrict_sstc_sstvala_"
    "sstvecd"
    "_ssu64xl_supm_svade_svbare_svinval_svnapot_svpbmt_svvptc_xandesperf_"
    "xandesvdot_xandesvqmac";
  static const std::vector<std::string> andes_66_float_addon = {
    "",            // v5
    "_zfa_zcf",    // v5f
    "_zfa_zcf_zcd" // v5d
  };
  static const std::vector<std::string> andes_66_atomic_addon = {
    "",                                  // elf toolchain
    "_za64rs_zaamo_zalrsc_zama16b_zawrs" // linux toolchain
  };

  // ARCH is expected to be in canonical form for the base extensions, e.g.,
  //   ok: rv32imfdc_...
  //   ng: rv32im_zifencei_fd_c_...
  constexpr std::string_view default_arch = ARCH;
  static_assert (!default_arch.empty(), "unexpected empty ARCH!");

  constexpr std::string_view base_arch =
      default_arch.substr(0, default_arch.find("_"));
  constexpr unsigned has_atomic = (base_arch.find("a") != std::string::npos);
  constexpr bool is_rv32 = (base_arch.find("rv32") != std::string::npos);

  auto get_float_level = [base_arch]() constexpr -> unsigned {
    if constexpr (base_arch.find("d") != std::string::npos)
      return 2;
    if constexpr (base_arch.find("f") != std::string::npos)
      return 1;
    return 0;
  };
  constexpr unsigned float_config = get_float_level();

  // new_arch = [andes_cpu_base]_[andes_cpu_*_addon]+
  std::string new_arch = "";
  if (isAndes23Series(cpu)) {
    new_arch += andes_23_base;
    new_arch += andes_23_float_addon[float_config];
  } else if (isAndes46Series(cpu)) {
    new_arch += andes_46_base;
    new_arch += andes_46_float_addon[float_config];
    new_arch += andes_46_atomic_addon[has_atomic];
  } else if (isAndes60Series(cpu)) {
    new_arch += andes_60_base;
    new_arch += andes_60_float_addon[float_config];
  } else if (isAndes66Series(cpu)) {
    new_arch += andes_66_base;
    new_arch += andes_66_float_addon[float_config];
    new_arch += andes_66_atomic_addon[has_atomic];
  } else {
    // Use the default ARCH.
    return false;
  }

  // new_arch = [andes_cpu_base]_[andes_cpu_*_addon]+_[andes_generic_suffix]
  new_arch += andes_generic_suffix;

  auto string_contains_p = [](const std::string &s, const char *ext) -> bool {
    if (s.find(ext) != std::string::npos)
      return true;
    return false;
  };

  // Extract all Zc from string s.
  // TODO: Optimize it to a compile-time string. It's tricky to do it with C++,
  //       so probably do it through configure.
  auto zc_scavenger = [](const std::string s) -> std::string {
    // This could be done by a single regex '(_zc[^_]+)+' or '(zc[^_]+_)+'
    // when the arch string is in canonical form. But implement it this
    // way can support chaotic arch strings like rv32if_zca_zifencei_zcf.
    std::regex re("zc[^_]+");
    std::smatch m;
    std::string result = "";
    std::string::const_iterator searchStart(s.cbegin());
    while (std::regex_search(searchStart, s.cend(), m, re)) {
      result += "_" + m[0].str();
      searchStart = m.suffix().first;
    }

    return result;
  };

  // new_arch =
  // [base_arch]_[andes_cpu_base]_[andes_cpu_*_addon]+_[andes_generic_suffix]
  if (string_contains_p(new_arch, "zca")) {
    // Remove RVC in base_arch, and use whatever Zc* defined in new_arch
    new_arch = "-march="
               + std::regex_replace(std::string(base_arch), std::regex("c"), "")
               + new_arch;
  } else {
    // Use the original base_arch and pick all Zc* from default_arch.
    new_arch = "-march="
               + std::string(base_arch)
               + zc_scavenger(std::string(default_arch))
               + new_arch;
  }

  // Post process
  if constexpr (!is_rv32)
    new_arch = std::regex_replace(new_arch, std::regex("_zcf"), "");

  new_arch = std::regex_replace(new_arch, std::regex("__+"), "_");

  arg_vec.push_back(new_arch);
  return true;
}

int main(int argc, const char * const argv[])
{
  char self_path[PATH_MAX];

  std::string exe_path = getMainExecutableImpl(nullptr, nullptr);

  strncpy(self_path, exe_path.c_str(), PATH_MAX);

  /* From man page,
   * Both dirname() and basename() may modify the contents of path, so it may
   * be desirable to pass a copy when calling one of these functions.  */
  char *dir = dirname(self_path);

  bool minimal = false;
  bool verbose = false;
  std::vector<std::string> extra_arg_vec;

  std::vector<std::string> all_args;
  all_args.assign(argv, argv + argc);

  std::string cpu;
  bool has_march = false;
  bool has_mext_vector = false;
  bool has_explicit_zvl = false;

  for (auto E = all_args.begin(); E != all_args.end();) {
    if (E->rfind("-mcpu=", 0) == 0) {
      // Only the last mcpu takes effect.
      cpu = E->substr(6);
    }
    if (E->rfind("-march=", 0) == 0) {
      has_march = true;
    }
    if (*E == "-v" || *E == "--verbose" || *E == "-###") {
      verbose = true;
    }
    if (E->rfind("-mext-vector", 0) == 0) {
      has_mext_vector = true;
      auto Pos = E->find('=');
      if (Pos != std::string::npos && Pos + 1 < E->size()) {
        has_explicit_zvl = true;
      }
    }
    if (*E == "--wrapper-minimal-mode") {
      minimal = true;
      // Consume the option
      E = all_args.erase(E);
      continue;
    }
    ++E;    
  }

  /* The priotity of march is: user specified > mcpu expansion > ARCH. */
  if (!has_march)
    has_march |= append_cpu_march(cpu, extra_arg_vec);
  /* Always add cpu -mext-* options to prevent multilib issues.  */
  append_cpu_options(cpu, extra_arg_vec);

#ifdef CLANGXX
  // Workaround: The wrapper option --wrapper-minimal-mode aims to call compiler
  // drivers without any optimization flags. However, as bs3 always calls Clang
  // through wrapper, we made Clang-wrapper neglect the option for now to match
  // the behavior before this patch.
  minimal = false;
#endif

  // Minimal-mode disables optimization flags and the default march (-march=ARCH)
  // but still allows expanding -mcpu to -march & -mext options.
  if (!minimal) {
    if (!has_march)
      extra_arg_vec.push_back("-march=" ARCH);

    // Extra optimization flags for both GXX & CLANGXX
    if (strcmp(LIBC, "mculib") == 0) {
      extra_arg_vec.push_back("-fno-math-errno");
    }

    // Extra optimization flags for CLANGXX
#ifdef CLANGXX
    extra_arg_vec.push_back("-ffinite-loops");
    extra_arg_vec.push_back("-ffp-contract=fast");
#endif

    // Extra optimization flags for GXX
#ifdef GXX
    extra_arg_vec.push_back("-mrvv-max-lmul=dynamic");
    if (!has_explicit_zvl) {
      extra_arg_vec.push_back("-mno-enable-unrolled-vls");
    }
#endif
  }

  /* Disable Zcmt table jump for Linux toolchains. */
  constexpr std::string_view target = TARGET;
  if constexpr (target.find("linux") != std::string::npos)
    extra_arg_vec.push_back("-mno-zcmt-table-jump");

#ifdef CLANGXX
  extra_arg_vec.push_back("-Wno-unused-command-line-argument");

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
    prog = "clang++" EXEEXT;
  } else {
    prog = "clang" EXEEXT;
  }
#endif
#ifdef GXX
  if (GXX) {
    prog = TARGET "-g++.gnu" EXEEXT;
  } else {
    prog = TARGET "-gcc.gnu" EXEEXT;
  }
#endif

  int new_argc = all_args.size() + extra_arg_vec.size();
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

  // Options specified in CMD should be added at the end of CMD, otherwise users
  // are not able to disable options implied by wrapper.
  for (int i = 1; i < all_args.size(); ++i) {
    new_args[i + sz] = all_args[i].c_str();
  }

  new_args[new_argc] = NULL;

#ifdef DEBUG
  printf ("self_path=\"%s\" %s cc_path=%s\n", dir, self_path, cc_path);
#endif

  if (verbose) {
    printf("AST_WRAPPER: ");
    for (int i = 0; i < new_argc; i++) {
      printf ("\"%s\" ", new_args[i]);
    }
    printf("\n");
  }

  fflush(stdout);

#ifndef _WIN32
  int rv = execvp (cc_path, (char* const*)new_args);

  if (rv < 0) {
    fprintf (stderr, "%s not found %s\n", prog, cc_path);
  }

  return rv;
#else
  struct pex_obj *pex;
  const char *err_msg;
  int pex_flags = PEX_USE_PIPES | PEX_LAST;
  int status = 0;
  int err = 0;
  int exit_code = -1;

  pex = pex_init (0, argv[0], NULL);

  if (pex == NULL) {
    fprintf (stderr, "%s fail to execute %s\n", prog, cc_path);
    return -1;
  }

  err_msg = pex_one (pex_flags, cc_path, (char* const*)new_args, NULL,
                     NULL, NULL, &status, &err);

  if (err_msg)
    fprintf (stderr, "Error running %s: %s\n", cc_path, err_msg);
  else if (status)
    {
      if (WIFSIGNALED (status))
        {
          int sig = WTERMSIG (status);
          fprintf (stderr, "%s terminated with signal %d [%s]%s\n",
                   cc_path, sig, strsignal (sig),
                   WCOREDUMP (status) ? ", core dumped" : "");
        }
      else if (WIFEXITED (status))
        exit_code = WEXITSTATUS (status);
    }
  else
    exit_code = 0;

  return exit_code;
#endif
}
