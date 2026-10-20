set(srcs "pylon_frame.c" "pylon_cmd.c")
set(priv_requires "")
if(NOT IDF_TARGET STREQUAL "linux")
    list(APPEND srcs "pylon_rs485.c")
    list(APPEND priv_requires esp_driver_uart esp_timer)
endif()

idf_component_register(SRCS ${srcs}
                       INCLUDE_DIRS "include"
                       REQUIRES limits pack_model
                       PRIV_REQUIRES ${priv_requires})
