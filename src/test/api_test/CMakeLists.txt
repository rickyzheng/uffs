SET (libapitest_SRCS 
		api_test.c
		api_test.h
	)

IF (UNIX)
	SET (libapitest_SRCS ${libapitest_SRCS} api_test_server_posix.c api_test_client_posix.c)
ENDIF()

IF (WIN32)
	SET (libapitest_SRCS ${libapitest_SRCS} api_test_server_win32.c api_test_client_win32.c)
ENDIF()

INCLUDE_DIRECTORIES(${uffs_SOURCE_DIR}/src/inc)
ADD_LIBRARY(apitest STATIC ${libapitest_SRCS} )

