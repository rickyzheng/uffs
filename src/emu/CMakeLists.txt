SET (libemu_SRCS 
		cmdline.c
		cmdline.h
		helper_cmds.c
		uffs_fileem.c
		uffs_fileem_share.c
		uffs_fileem_wrap.c
		uffs_fileem_ecc_soft.c
		uffs_fileem_ecc_hw.c
		uffs_fileem_ecc_hw_auto.c
		uffs_fileem.h
		test_cmds.c
		api_srv.c
		api_srv.h
	)

IF (UNIX)
	SET (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DENABLE_API_SRV")
	SET (CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DENABLE_API_SRV")
ENDIF()

INCLUDE_DIRECTORIES(${uffs_SOURCE_DIR}/src/platform/posix)
INCLUDE_DIRECTORIES(${uffs_SOURCE_DIR}/src/inc)
ADD_LIBRARY(emu STATIC ${libemu_SRCS} )

