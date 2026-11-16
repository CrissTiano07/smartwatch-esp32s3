# CMakeLists.txt — arquivo raiz do projeto ESP-IDF.
# O CMake lê este arquivo primeiro para saber como montar o firmware.

# Versão mínima do CMake exigida pelo ESP-IDF 5.x.
cmake_minimum_required(VERSION 3.16)

# Puxa todo o sistema de build do ESP-IDF (componentes, toolchain, menuconfig).
# $ENV{IDF_PATH} vem do export.bat: C:\esp\v5.5.3\esp-idf
include($ENV{IDF_PATH}/tools/cmake/project.cmake)

# Nome do projeto. Vira o nome do binário: build/smartwatch-esp32s3.bin
project(smartwatch-esp32s3)
