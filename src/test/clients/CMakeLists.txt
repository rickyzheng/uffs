INCLUDE_DIRECTORIES(${uffs_SOURCE_DIR}/src/inc)
INCLUDE_DIRECTORIES(${uffs_SOURCE_DIR}/src/emu)

SET(example_SRCS example.c ${uffs_SOURCE_DIR}/src/uffs/uffs_crc.c ${uffs_SOURCE_DIR}/src/emu/api_srv_public.c)
ADD_EXECUTABLE(example ${example_SRCS})

