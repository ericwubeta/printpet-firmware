idf_component_register(SRCS "hooks.c" REQUIRES bootloader_support esp_rom hal spi_flash)
target_include_directories(${COMPONENT_LIB} PRIVATE
    "${IDF_PATH}/components/bootloader_support/private_include")
target_link_libraries(${COMPONENT_LIB} INTERFACE "-u bootloader_hooks_include")
