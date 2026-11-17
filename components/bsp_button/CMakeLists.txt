# components/bsp_button/CMakeLists.txt — botão físico BOOT (GPIO0).
# Lê o botão por interrupção de GPIO; a ISR só avisa uma task por uma fila.

idf_component_register(
    SRCS "bsp_button.c"
    INCLUDE_DIRS "include"
    REQUIRES
        driver        # driver/gpio.h — configurar o pino, a interrupção e ler o nível
)
