INCLUDE_DIRECTORIES(${uffs_SOURCE_DIR}/src/inc)
INCLUDE_DIRECTORIES(${uffs_SOURCE_DIR}/src/emu)

LINK_DIRECTORIES(${uffs_BINARY_DIR}/src/emu)
LINK_DIRECTORIES(${uffs_BINARY_DIR}/src/uffs)

SET(static_mem_SRCS static-mem-allocate.c)
ADD_EXECUTABLE(static-mem-example ${static_mem_SRCS})
TARGET_LINK_LIBRARIES(static-mem-example emu uffs emu)
