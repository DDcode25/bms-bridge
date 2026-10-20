idf_component_register(SRCS "test_main.c" "test_pylon_frame.c" "test_pylon_cmd.c"
                            "test_limits.c" "test_pack.c" "test_bmz_can.c" "fixtures.c"
                       INCLUDE_DIRS "."
                       PRIV_REQUIRES unity pylon_rs485 limits pack_model bmz_driver)
