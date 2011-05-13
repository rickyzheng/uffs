CMAKE_MINIMUM_REQUIRED(VERSION 2.6 )
PROJECT( uffs )

SET(CMAKE_CXX_FLAGS_VGL "-O0 -g")
SET(CMAKE_C_FLAGS_VGL "-O0 -g")

SET(CMAKE_C_FLAGS_GCOV "-g -O0 -Wall -fprofile-arcs -ftest-coverage")
SET(CMAKE_CXX_FLAGS_GCOV "-g -O0 -Wall -fprofile-arcs -ftest-coverage")
SET(CMAKE_EXE_LINKER_FLAGS_GCOV "${CMAKE_EXE_LINKER_FLAGS} -fprofile-arcs -ftest-coverage -lgcov")

IF (CMAKE_COMPILER_IS_GNUCC)
	SET(CMAKE_CXX_FLAGS "-Wall -Werror")
	SET(CMAKE_C_FLAGS "-Wall -Werror")
ENDIF()

ADD_SUBDIRECTORY( src )

