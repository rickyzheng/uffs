SET (libposix_SRCS 
		${uffs_SOURCE_DIR}/src/platform/posix/uffs_os_posix.c
		${uffs_SOURCE_DIR}/src/platform/posix/uffs_config.h
	)

INCLUDE_DIRECTORIES(${uffs_SOURCE_DIR}/src/inc)
ADD_LIBRARY(platform_posix STATIC ${libposix_SRCS} )

