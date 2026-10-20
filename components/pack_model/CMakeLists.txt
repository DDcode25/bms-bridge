set(srcs "pack_aggregate.c")
set(requires "")
if(NOT IDF_TARGET STREQUAL "linux")
    list(APPEND srcs "pack_store.c")
    list(APPEND requires freertos)
endif()

idf_component_register(SRCS ${srcs}
                       INCLUDE_DIRS "include"
                       PRIV_REQUIRES ${requires})
