# main/CMakeLists.txt — registra o componente "main".
# No ESP-IDF, todo código fica em "componentes"; "main" é o componente
# obrigatório onde está a função app_main().

idf_component_register(
    SRCS "main.c"                # arquivos .c que fazem parte deste componente
    INCLUDE_DIRS "."             # pasta onde ficam os .h deste componente
    # Ao declarar REQUIRES, o "main" deixa de depender de TODOS os componentes
    # automaticamente, então listamos aqui o que ele realmente usa:
    PRIV_REQUIRES
        esp_psram               # esp_psram_get_size() — log da PSRAM no boot
        spi_flash               # esp_flash.h — esp_flash_get_size()
        esp_timer               # esp_timer.h — esp_timer_get_time()
        bsp_i2c                 # nosso barramento I2C
        axp2101                 # nosso driver de energia
        pcf85063                # nosso driver do RTC
        bsp_display             # nossa tela AMOLED (CO5300/QSPI)
)
