idf_component_register(SRCS "main.c" "console_cmds.c"
                       INCLUDE_DIRS "."
                       PRIV_REQUIRES bmz_driver pack_model limits pylon_rs485
                                     console esp_timer)
