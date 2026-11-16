# components/bsp_i2c/CMakeLists.txt — dono único do barramento I2C da placa.
#
# REQUIRES esp_driver_i2c: o header público (bsp_i2c.h) usa tipos do
# driver i2c_master, então quem incluir bsp_i2c.h também precisa enxergá-lo.

idf_component_register(
    SRCS "bsp_i2c.c"
    INCLUDE_DIRS "include"
    REQUIRES esp_driver_i2c
)
