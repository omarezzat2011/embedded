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
#include <semphr.h>

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

// Buzzer States
typedef enum {
    NOT_PLAYING,
    PLAYING_SUCCESS_TONE,
    PLAYING_CLOSE_TONE,
    PLAYING_ERROR_TONE
} buzzer_state_t;

// Global Buzzer State
volatile buzzer_state_t buzzer_state = NOT_PLAYING;  // Volatile ensures correct visibility across tasks
// Buzzer States
typedef enum {
    CONSTANT,
    OPENING,
    CLOSING
} servo_state_t;

volatile servo_state_t servo_state = CONSTANT;  // Volatile ensures correct visibility across tasks

void buzzer_task(void *pvParameters) {
    while (1) {
        // Check if the buzzer state is to play a sound
        if (buzzer_state == PLAYING_SUCCESS_TONE) {
            buzzer_play_tone(BUZZER_PIN, 1000, 1000); // Play 1kHz tone for 1 second
            buzzer_state = NOT_PLAYING; // Reset the state
        } 
        else if (buzzer_state == PLAYING_ERROR_TONE) {
            buzzer_play_tone(BUZZER_PIN, 500, 1000); // Play 500Hz tone for 1 second
            buzzer_state = NOT_PLAYING; // Reset the state
        }
        else if ( buzzer_state == PLAYING_CLOSE_TONE ){
            buzzer_play_tone(BUZZER_PIN, 1500, 1000); // 1.5kHz tone for 1000ms
            buzzer_state = NOT_PLAYING; // Reset the state


        }

        vTaskDelay(pdMS_TO_TICKS(200)); // Small delay to avoid busy-waiting
    }
}
void servo_task(void *pvParameters) {
    while (1) {
        // Check if the buzzer state is to play a sound
        if (servo_state == OPENING) {
            open_servo(pwm_gpio_to_slice_num(SERVO_PIN)); // Rotate the servo
            vTaskDelay(pdMS_TO_TICKS(1000)); // Wait for servo to complete opening

            servo_state = CONSTANT; // Reset the state
        } 
        else if (servo_state == CLOSING) {
            close_servo(pwm_gpio_to_slice_num(SERVO_PIN)); // Rotate the servo
            vTaskDelay(pdMS_TO_TICKS(1000)); // Wait for servo to complete opening

            servo_state = CONSTANT; // Reset the state
        }
       

        vTaskDelay(pdMS_TO_TICKS(200)); // Small delay to avoid busy-waiting
    }
}
void all_task(void *pvParameters) {
    // init part
    MFRC522Ptr_t mfrc;
    uint8_t uid[10];
    uint8_t uid_length;
    char rfid_message[32];
        // Initialize the LCD
    lcd_init();

    rfid_init(&mfrc);
    buzzer_init(BUZZER_PIN);
    servo_init(SERVO_PIN);
    ir_sensor_init_adc(IR_SENSOR_ADC_PIN);

    hc_sr04_init(TRIG_PIN, ECHO_PIN);
    // Display a ready message on the LCD
    lcd_send_byte(LCD_CLEAR, true);
    vTaskDelay(pdMS_TO_TICKS(500));
    lcd_print("System Ready!");
    vTaskDelay(pdMS_TO_TICKS(2000));
    lcd_send_byte(LCD_CLEAR, true);




   while (1) {
        lcd_send_byte(LCD_CLEAR, true);
        vTaskDelay(pdMS_TO_TICKS(500));
        lcd_print("Scan RFID Card!");
        vTaskDelay(pdMS_TO_TICKS(2000));
        lcd_send_byte(LCD_CLEAR, true);

       if (card_present(mfrc)) {
        if (read_card_uid(mfrc, uid, &uid_length)) {
            // Format the UID as a string for display
            char uid_str[30];
            sprintf(uid_str, "UID: ");
            for (uint8_t i = 0; i < uid_length; i++) {
                sprintf(uid_str + strlen(uid_str), "%02X ", uid[i]); // Append each byte in hex
            }
            lcd_send_byte(LCD_CLEAR, true);
            vTaskDelay(pdMS_TO_TICKS(100));
            lcd_print(uid_str);
            vTaskDelay(pdMS_TO_TICKS(1000));
            lcd_send_byte(LCD_CLEAR, true);
            if (uid_length == 4 && uid[0] == 0x07 && uid[1] == 0x20 && uid[2] == 0xD1 && uid[3] == 0x26) {
                // Matched UID - Play accessible sound
                printf("Access Granted. Playing success tone...\n");
                uint16_t ir_raw_value = ir_sensor_read_adc();
                float ir_voltage = ir_sensor_get_voltage(ir_raw_value);

                char ir_message[32];
                if (ir_voltage > 3.3 || ir_voltage < 0) {
                    strcpy(ir_message, "Invalid IR");
                } else {
                    if(ir_voltage < 1.5){
                        buzzer_state = PLAYING_ERROR_TONE;  // Set state to play success tone
                    }else{    
                        buzzer_state = PLAYING_SUCCESS_TONE;  // Set state to play success tone
                        servo_state  = OPENING;
                        float distance = measure_distance(TRIG_PIN, ECHO_PIN);
                        char us_message[32];
                        if (distance < 0) {
                            sprintf(us_message, "No Obj Detected");
                            printf("Ultrasonic: Timeout! No object detected.\n");
                        } else {
                            sprintf(us_message, "Dist: %.2fcm", distance);
                            printf("Ultrasonic: %.2f cm\n", distance);
                        }
                        lcd_send_byte(LCD_CLEAR, true);
                        vTaskDelay(pdMS_TO_TICKS(100));
                        lcd_print(uid_str);
                        vTaskDelay(pdMS_TO_TICKS(2000));
                        lcd_send_byte(LCD_CLEAR, true);
                        while(distance>10||distance<0){
                            distance = measure_distance(TRIG_PIN, ECHO_PIN);
                        }
                        buzzer_state = PLAYING_CLOSE_TONE;  // Set state to play success tone
                        servo_state  = CLOSING;
                    }
                }
            } else {
                // Unmatched UID - Play non-accessible sound
                printf("Access Denied. Playing error tone...\n");
                buzzer_state = PLAYING_ERROR_TONE;  // Set state to play success tone
            }
           sleep_ms(5000);

        }
    }
    }
        vTaskDelay(pdMS_TO_TICKS(2000));
    

}


int main() {
    stdio_init_all(); // Initialize stdio for debugging
    

    // Create tasks
    xTaskCreate(all_task, "RFID and Sensor Task", 8192, NULL, 1, NULL);
    xTaskCreate(buzzer_task, "Buzzer Task", 2048, NULL, 1, NULL);  
    xTaskCreate(servo_task, "Servo Task", 2048, NULL, 1, NULL);  


    // Start FreeRTOS scheduler
    vTaskStartScheduler();

    // The program should never reach here
    while (1) {
        // Should not run
    }

    return 0;
}