# main/CMakeLists.txt — registra o componente "main".
# No ESP-IDF, todo código fica em "componentes"; "main" é o componente
# obrigatório onde está a função app_main().

idf_component_register(
    SRCS "main.c"        # arquivos .c que fazem parte deste componente
    INCLUDE_DIRS "."     # pasta onde ficam os .h deste componente
)
