# components/bsp_display/CMakeLists.txt — componente da tela (CO5300/QSPI).
# A dependência do driver (waveshare/esp_lcd_sh8601) está no idf_component.yml;
# o gerenciador de componentes a baixa e a adiciona ao build.
idf_component_register(
    SRCS "bsp_display.c"
    INCLUDE_DIRS "include"
    REQUIRES esp_lcd esp_driver_spi   # painel/IO (esp_lcd) e barramento SPI
    PRIV_REQUIRES ui_fonts            # fonte própria dos dígitos (font_digits_180)
)
