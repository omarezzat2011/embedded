add_executable(${ProjectName}
    main.c
    drivers/buzzerDriver/buzzer.c
    drivers/servoDriver/servo.c
    drivers/irDriver/ir_sensor.c
    drivers/lcdDriver/lcd.c
    drivers/ultrasonicDriver/ultrasonic.c
    drivers/rfidDriver/rfid.c
    drivers/rfidDriver/mfrc522.c
)

target_include_directories(${ProjectName} PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}
)

target_link_libraries(${ProjectName} 
    pico_stdlib 
    FreeRTOS-Kernel-Heap4 
    hardware_spi
    hardware_adc
    hardware_gpio
    hardware_timer
    )

pico_add_extra_outputs(${ProjectName})