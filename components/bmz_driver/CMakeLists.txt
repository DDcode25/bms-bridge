set(srcs "bmz_can.c")
set(priv_requires "")
if(NOT IDF_TARGET STREQUAL "linux")
    list(APPEND srcs "bmz_driver.c")
    list(APPEND priv_requires driver esp_timer)
endif()

idf_component_register(SRCS ${srcs}
                       INCLUDE_DIRS "include"
                       REQUIRES pack_model
                       PRIV_REQUIRES ${priv_requires})
