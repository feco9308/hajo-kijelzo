cmake_minimum_required(VERSION 3.22)
set(CMAKE_TOOLCHAIN_FILE "${CMAKE_CURRENT_LIST_DIR}/cmake/arm-none-eabi.cmake" CACHE FILEPATH "")
project(sharp_lcd C ASM)
option(SHARP_DIAGNOSTIC_LOOP "Repeat DISP/clear/white/black/checkerboard diagnosis" ON)
option(SHARP_SIMPLE_BLINK "Unambiguous 2-second white/black LCD test" ON)
set(CMAKE_C_STANDARD 11)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
# Confirmed by user: STM32G431CBU6, 128 KiB flash, UFQFPN48.
set(HAL Drivers/STM32G4xx_HAL_Driver)
set(DEVICE Drivers/CMSIS/Device/ST/STM32G4xx)
set(USBD Drivers/STM32_USB_Device_Library)
add_executable(sharp_lcd
    Core/Src/main.c Core/Src/sharp_ls044q7dh01.c
    ${DEVICE}/Source/Templates/system_stm32g4xx.c
    ${DEVICE}/Source/Templates/gcc/startup_stm32g431xx.s)
target_sources(sharp_lcd PRIVATE USB/Src/usbd_conf.c USB/Src/usbd_desc.c USB/Src/usb_diag.c
    ${USBD}/Core/Src/usbd_core.c ${USBD}/Core/Src/usbd_ctlreq.c ${USBD}/Core/Src/usbd_ioreq.c
    ${USBD}/Class/CDC/Src/usbd_cdc.c ${HAL}/Src/stm32g4xx_ll_usb.c)
foreach(module hal hal_cortex hal_dma hal_flash hal_flash_ex hal_gpio hal_pwr hal_pwr_ex hal_rcc hal_rcc_ex hal_spi hal_tim hal_tim_ex hal_pcd hal_pcd_ex)
    target_sources(sharp_lcd PRIVATE ${HAL}/Src/stm32g4xx_${module}.c)
endforeach()
target_include_directories(sharp_lcd PRIVATE Core/Inc ${HAL}/Inc ${DEVICE}/Include Drivers/CMSIS_5/CMSIS/Core/Include
    USB/Inc ${USBD}/Core/Inc ${USBD}/Class/CDC/Inc)
target_compile_definitions(sharp_lcd PRIVATE STM32G431xx USE_HAL_DRIVER)
if(SHARP_DIAGNOSTIC_LOOP)
    target_compile_definitions(sharp_lcd PRIVATE SHARP_DIAGNOSTIC_LOOP)
endif()
if(SHARP_SIMPLE_BLINK)
    target_compile_definitions(sharp_lcd PRIVATE SHARP_SIMPLE_BLINK)
endif()
target_compile_options(sharp_lcd PRIVATE -mcpu=cortex-m4 -mthumb -mfpu=fpv4-sp-d16 -mfloat-abi=hard -Og -g3 -ffunction-sections -fdata-sections -Wall -Wextra -Werror)
target_link_options(sharp_lcd PRIVATE -mcpu=cortex-m4 -mthumb -mfpu=fpv4-sp-d16 -mfloat-abi=hard -nostartfiles
    -T${CMAKE_CURRENT_SOURCE_DIR}/STM32G431CBU6_FLASH.ld --specs=nano.specs --specs=nosys.specs
    -Wl,--gc-sections,-Map=${CMAKE_CURRENT_BINARY_DIR}/sharp_lcd.map -Wl,--print-memory-usage)
set_target_properties(sharp_lcd PROPERTIES SUFFIX .elf LINK_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/STM32G431CBU6_FLASH.ld)
add_custom_command(TARGET sharp_lcd POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} -O ihex $<TARGET_FILE:sharp_lcd> ${CMAKE_CURRENT_BINARY_DIR}/sharp_lcd.hex
    COMMAND ${CMAKE_OBJCOPY} -O binary $<TARGET_FILE:sharp_lcd> ${CMAKE_CURRENT_BINARY_DIR}/sharp_lcd.bin
    COMMAND ${CMAKE_SIZE} $<TARGET_FILE:sharp_lcd>)
