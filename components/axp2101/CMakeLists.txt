# components/axp2101/CMakeLists.txt — driver mínimo do chip de energia.
# Só fala com o hardware através do bsp_i2c (barramento compartilhado).

idf_component_register(
    SRCS "axp2101.c"
    INCLUDE_DIRS "include"
    REQUIRES bsp_i2c
)
