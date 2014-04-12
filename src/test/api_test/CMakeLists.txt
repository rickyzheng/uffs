SET (libapitest_server_SRCS 
		api_test.c
		api_test.h
	)

SET (libapitest_client_SRCS
		api_test.c
		api_test.h
		api_test_client_wrapper.c
		${uffs_SOURCE_DIR}/src/uffs/uffs_crc.c
	)

IF (UNIX)
	SET (libapitest_server_SRCS ${libapitest_server_SRCS} api_test_server_posix.c)
	SET (libapitest_client_SRCS ${libapitest_client_SRCS} api_test_client_posix.c)
ENDIF()

IF (WIN32)
	SET (libapitest_server_SRCS ${libapitest_server_SRCS} api_test_server_win32.c)
	SET (libapitest_client_SRCS ${libapitest_client_SRCS} api_test_client_win32.c)
ENDIF()

SET (libapitest_sqlite3_SRCS
		${libapitest_client_SRCS}
		os_uffs.c
		os_uffs.h
	)

set (srcs)
foreach (file ${libapitest_server_SRCS})
	set (srcs ${srcs} ${CMAKE_CURRENT_SOURCE_DIR}/${file})
endforeach(file)
set (lib_SRCS ${lib_SRCS} ${srcs} PARENT_SCOPE)

ADD_LIBRARY(apitest_server STATIC ${libapitest_server_SRCS})
ADD_LIBRARY(apitest_client STATIC ${libapitest_client_SRCS})
IF (UNIX)
	ADD_LIBRARY(apitest_sqlite3 SHARED ${libapitest_sqlite3_SRCS})
ENDIF()



