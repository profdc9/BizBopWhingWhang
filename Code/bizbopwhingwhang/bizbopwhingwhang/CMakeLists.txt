add_executable(bizbopwhingwhang
    src/main.cpp
    src/tinycl.cpp
)

target_include_directories(bizbopwhingwhang PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/src
)

target_link_libraries(bizbopwhingwhang
    pico_stdlib
    hardware_pio
    hardware_dma
    pico_multicore
    hardware_interp
    hardware_pwm
    hardware_adc
    hardware_i2c
    hardware_spi
    hardware_flash
    )

# create map/bin/hex file etc.
pico_add_extra_outputs(bizbopwhingwhang)

# Allow stdio to USB
pico_enable_stdio_usb(bizbopwhingwhang 1)
