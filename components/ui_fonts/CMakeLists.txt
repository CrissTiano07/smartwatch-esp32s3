# components/ui_fonts/CMakeLists.txt — fontes próprias da UI (geradas por lv_font_conv).
# A dependência do LVGL está no idf_component.yml (vira REQUIRES automático),
# por isso o .c da fonte enxerga "lvgl.h" e os símbolos lv_font_* ao compilar.
idf_component_register(
    SRCS "font_digits_180.c"   # dígitos 0-9, Montserrat Regular 180 px, 4 bpp
    INCLUDE_DIRS "include"
)
