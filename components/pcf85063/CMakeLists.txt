# components/pcf85063/CMakeLists.txt — driver do RTC (relógio de tempo real).
# Como o axp2101, só fala com o hardware através do bsp_i2c.

idf_component_register(
    SRCS "pcf85063.c"
    INCLUDE_DIRS "include"
    REQUIRES bsp_i2c
)
