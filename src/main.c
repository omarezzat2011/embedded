#include "FreeRTOS.h"
#include "task.h"
#include <stdio.h>
#include "pico/stdlib.h"

#include "hardware/spi.h"
#include "drivers/lcdDriver/lcd.h"
#include "drivers/rfidDriver/rfid.h"
#include "drivers/rfidDriver/mfrc522.h"


#include "drivers/buzzerDriver/buzzer.h"
#include "drivers/servoDriver/servo.h"
#include "drivers/irDriver/ir_sensor.h"
#include "drivers/lcdDriver/lcd.h"
#include "drivers/ultrasonicDriver/ultrasonic.h"
#include "../include/hardware/gpio.h"
#include "pwm.h"
#include <string.h>

#define BUZZER_PIN 26      // Buzzer GPIO pin
#define SERVO_PIN 20       // Servo GPIO pin
#define IR_SENSOR_ADC_PIN 27 // IR sensor ADC pin
#define TRIG_PIN 29      // Ultrasonic sensor TRIG pin
#define ECHO_PIN 28         // Ultrasonic sensor ECHO pin

// RFID Pin Definitions
#define SDA_PIN 21 // D9
#define SCK_PIN 6  // D13
#define MOSI_PIN 7 // D11
#define MISO_PIN 4 // D12
#define RST_PIN 5  // D10


// Global variables for shared data
volatile bool access_granted = false;
volatile float distance = 0.0f;

// Task: RFID and Servo Control
void rfid_task(void *pvParameters) {
    // Initialize RFID
    MFRC522Ptr_t mfrc;
    rfid_init(&mfrc);

    // Initialize Servo
    servo_init(SERVO_PIN);

    uint8_t uid[10];
    uint8_t uid_length;

    for (;;) {
        printf("Waiting for RFID card...\n");

        if (card_present(&mfrc)) {
            if (read_card_uid(&mfrc, uid, &uid_length)) {
                printf("Card detected!\n");

                if (uid_length == 4 && uid[0] == 0x07 && uid[1] == 0x20 && uid[2] == 0xD1 && uid[3] == 0x26) {
                    printf("Access Granted\n");
                    access_granted = true;

                    rotate_servo(pwm_gpio_to_slice_num(SERVO_PIN)); // Open the gate
                    vTaskDelay(pdMS_TO_TICKS(5000)); // Keep gate open for 5 seconds
                    rotate_servo(pwm_gpio_to_slice_num(SERVO_PIN)); // Close the gate
                } else {
                    printf("Access Denied\n");
                    access_granted = false;
                }
            }
        }

        vTaskDelay(pdMS_TO_TICKS(2000)); // Check for cards every 2 seconds
    }
}

// Task: IR and Ultrasonic Sensors
void sensor_task(void *pvParameters) {
    // Initialize IR Sensor
    ir_sensor_init_adc(IR_SENSOR_ADC_PIN);

    // Initialize Ultrasonic Sensor
    hc_sr04_init(TRIG_PIN, ECHO_PIN);

    for (;;) {
        // Read IR Sensor
        uint16_t ir_raw_value = ir_sensor_read_adc();
        float ir_voltage = ir_sensor_get_voltage(ir_raw_value);

        if (ir_voltage > 3.3 || ir_voltage < 0) {
            printf("Invalid IR Reading\n");
        } else {
            printf("IR Voltage: %.2fV\n", ir_voltage);
        }

        // Read Ultrasonic Sensor
        distance = measure_distance(TRIG_PIN, ECHO_PIN);
        if (distance < 0) {
            printf("No Object Detected\n");
        } else {
            printf("Ultrasonic Distance: %.2f cm\n", distance);
        }

        vTaskDelay(pdMS_TO_TICKS(2000)); // Delay for 2 seconds
    }
}

// Task: LCD Display
void lcd_task(void *pvParameters) {
    // Initialize LCD
    lcd_init();

    for (;;) {
        lcd_send_byte(LCD_CLEAR, true);

        if (access_granted) {
            lcd_print("Access Granted");
        } else {
            char dist_message[32];
            snprintf(dist_message, sizeof(dist_message), "Dist: %.2f cm", distance);
            lcd_print(dist_message);
        }

        vTaskDelay(pdMS_TO_TICKS(1000)); // Update every 1 second
    }
}

// Task: Buzzer Control
void buzzer_task(void *pvParameters) {
    // Initialize Buzzer
    buzzer_init(BUZZER_PIN);

    for (;;) {
        if (access_granted) {
            buzzer_play_tone(BUZZER_PIN, 1000, 500); // 1kHz tone for 500ms
        }

        vTaskDelay(pdMS_TO_TICKS(3000)); // Check every 3 seconds
    }
}

int main() {
    stdio_init_all(); // Initialize stdio for debugging

    // Create tasks
    xTaskCreate(rfid_task, "RFID Task", 1024, NULL, 1, NULL);
    xTaskCreate(sensor_task, "Sensor Task", 1024, NULL, 1, NULL);
    xTaskCreate(lcd_task, "LCD Task", 1024, NULL, 1, NULL);
    xTaskCreate(buzzer_task, "Buzzer Task", 512, NULL, 1, NULL);

    // Start FreeRTOS scheduler
    vTaskStartScheduler();

    // The program should never reach here
    while (1) {
        // Should not run
    }

    return 0;
}