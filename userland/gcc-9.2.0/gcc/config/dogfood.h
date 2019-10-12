/* Useful if you wish to make target-specific GCC changes. */
#undef TARGET_DOGFOOD
#define TARGET_DOGFOOD 1
 
#undef LIB_SPEC
#define LIB_SPEC "-lc"
 
/* Files that are linked before user code.
   The %s tells GCC to look for these files in the library directory. */
#undef STARTFILE_SPEC
#define STARTFILE_SPEC "crt0.o%s crti.o%s crtbegin.o%s"
 
/* Files that are linked after user code. */
#undef ENDFILE_SPEC
#define ENDFILE_SPEC "crtend.o%s crtn.o%s"
 
/* Additional predefined macros. */
#undef TARGET_OS_CPP_BUILTINS
#define TARGET_OS_CPP_BUILTINS()      \
  do {                                \
    builtin_define_std ("unix");         \
    builtin_define ("__dogfood__");    \
    builtin_assert ("system=unix");      \
    builtin_assert ("system=dogfood"); \
  } while(0);
