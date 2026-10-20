# Хостовые unit-тесты (ТЗ 6.2): кодек Pylontech, лимиты, сводка, разбор CAN.
#   idf.py --preview set-target linux && idf.py build && ./build/bms_bridge_test.elf
cmake_minimum_required(VERSION 3.16)
set(EXTRA_COMPONENT_DIRS ../components)
set(COMPONENTS main)
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(bms_bridge_test)
