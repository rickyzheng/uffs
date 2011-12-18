INCLUDE_DIRECTORIES(${uffs_SOURCE_DIR}/src/inc)
INCLUDE_DIRECTORIES(${uffs_SOURCE_DIR}/src/emu)
INCLUDE_DIRECTORIES(${uffs_SOURCE_DIR}/src/test/api_test)

SET(example_SRCS example.c ${uffs_SOURCE_DIR}/src/uffs/uffs_crc.c)
ADD_EXECUTABLE(example ${example_SRCS})
TARGET_LINK_LIBRARIES(example apitest)

