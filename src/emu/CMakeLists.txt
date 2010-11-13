SET(libemu_SRCS cmdline.c cmdline.h helper_cmds.c uffs_fileem.c uffs_fileem_share.c uffs_fileem_wrap.c uffs_fileem_ecc_soft.c uffs_fileem_ecc_hw.c uffs_fileem_ecc_hw_auto.c uffs_fileem.h uffs_os_posix.c test_cmds.c)

INCLUDE_DIRECTORIES(${uffs_SOURCE_DIR}/src/inc)
ADD_LIBRARY(emu STATIC ${libemu_SRCS} )

