cmd_scripts/basic/docproc := gcc -Wp,-MD,scripts/basic/.docproc.d  -Wall -Wstrict-prototypes -O2 -fomit-frame-pointer       -o scripts/basic/docproc scripts/basic/docproc.c  

deps_scripts/basic/docproc := \
  scripts/basic/docproc.c \
  /usr/include/stdio.h \
    $(wildcard include/config/gefile64.h) \
    $(wildcard include/config/d.h) \
    $(wildcard include/config/ix.h) \
    $(wildcard include/config/.h) \
    $(wildcard include/config/c99.h) \
    $(wildcard include/config/en.h) \
    $(wildcard include/config/ix2.h) \
    $(wildcard include/config/en2k8.h) \
    $(wildcard include/config/e/offset64.h) \
    $(wildcard include/config/ile.h) \
    $(wildcard include/config/c.h) \
    $(wildcard include/config/x98.h) \
    $(wildcard include/config/en2k.h) \
    $(wildcard include/config/gefile.h) \
    $(wildcard include/config/ern/inlines.h) \
    $(wildcard include/config/tify/level.h) \
  /usr/include/features.h \
    $(wildcard include/config/c95.h) \
    $(wildcard include/config/ix199309.h) \
    $(wildcard include/config/ix199506.h) \
    $(wildcard include/config/en/extended.h) \
    $(wildcard include/config/en2kxsi.h) \
    $(wildcard include/config/en2k8xsi.h) \
    $(wildcard include/config/ntrant.h) \
    $(wildcard include/config/i.h) \
    $(wildcard include/config/ix/implicitly.h) \
    $(wildcard include/config/ern/inlines/in/libc.h) \
  /usr/include/bits/predefs.h \
  /usr/include/sys/cdefs.h \
    $(wildcard include/config/espaces.h) \
  /usr/include/bits/wordsize.h \
  /usr/include/gnu/stubs.h \
  /usr/include/gnu/stubs-32.h \
  /usr/lib/gcc/i686-linux-gnu/4.4.5/include/stddef.h \
  /usr/include/bits/types.h \
  /usr/include/bits/typesizes.h \
  /usr/include/libio.h \
    $(wildcard include/config/a.h) \
    $(wildcard include/config/ar/t.h) \
    $(wildcard include/config//io/file.h) \
  /usr/include/_G_config.h \
  /usr/include/wchar.h \
  /usr/lib/gcc/i686-linux-gnu/4.4.5/include/stdarg.h \
  /usr/include/bits/stdio_lim.h \
  /usr/include/bits/sys_errlist.h \
  /usr/include/bits/stdio.h \
  /usr/include/bits/stdio2.h \
  /usr/include/stdlib.h \
  /usr/include/bits/waitflags.h \
  /usr/include/bits/waitstatus.h \
  /usr/include/endian.h \
  /usr/include/bits/endian.h \
  /usr/include/bits/byteswap.h \
  /usr/include/sys/types.h \
  /usr/include/time.h \
  /usr/include/sys/select.h \
  /usr/include/bits/select.h \
  /usr/include/bits/sigset.h \
  /usr/include/bits/time.h \
  /usr/include/sys/sysmacros.h \
  /usr/include/bits/pthreadtypes.h \
  /usr/include/alloca.h \
  /usr/include/bits/stdlib.h \
  /usr/include/string.h \
    $(wildcard include/config/ing/inlines.h) \
  /usr/include/xlocale.h \
  /usr/include/bits/string.h \
  /usr/include/bits/string2.h \
    $(wildcard include/config/ing/arch/strchr.h) \
  /usr/include/bits/string3.h \
  /usr/include/ctype.h \
  /usr/include/unistd.h \
  /usr/include/bits/posix_opt.h \
  /usr/include/bits/environments.h \
  /usr/include/bits/confname.h \
  /usr/include/getopt.h \
  /usr/include/bits/unistd.h \
  /usr/lib/gcc/i686-linux-gnu/4.4.5/include-fixed/limits.h \
  /usr/lib/gcc/i686-linux-gnu/4.4.5/include-fixed/syslimits.h \
  /usr/include/limits.h \
  /usr/include/bits/posix1_lim.h \
  /usr/include/bits/local_lim.h \
  /usr/include/linux/limits.h \
  /usr/include/bits/posix2_lim.h \
  /usr/include/sys/wait.h \
  /usr/include/signal.h \
  /usr/include/bits/signum.h \
  /usr/include/bits/siginfo.h \
  /usr/include/bits/sigaction.h \
  /usr/include/bits/sigcontext.h \
  /usr/include/bits/sigstack.h \
  /usr/include/sys/ucontext.h \
  /usr/include/bits/sigthread.h \
  /usr/include/sys/resource.h \
  /usr/include/bits/resource.h \

scripts/basic/docproc: $(deps_scripts/basic/docproc)

$(deps_scripts/basic/docproc):
