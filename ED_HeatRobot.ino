//
//
/*
    Name:       ED_HeatRobot.ino
    Created:	8/10/2022 4:52:56 μμ
    Author:     ED
*/

// Servo library for the control of 2 onboard servo motors.

#include "esp_camera.h"
#include <WiFi.h>

// Select different ESP32 timer number (0-3) to avoid conflict
#define USE_ESP32_TIMER_NO          3

// To be included only in main(), .ino with setup() to avoid `Multiple Definitions` Linker Error
#define TIMER_INTERRUPT_DEBUG       1
#define ISR_SERVO_DEBUG             1
#include "ESP32_ISR_Servo.h"
int x_servo = -1;
int y_servo = -1;

void startWebServer();
const char* ssid = "OnePlus 9"; //OnePlus 9
const char* password = "12345678"; //12345678

// The rotator class controlling the motors (based on the screen resolution)
#define initial_y 45
#define initial_x 0

void setup() {
  ESP32_ISR_Servos.useTimer(USE_ESP32_TIMER_NO);
  x_servo = ESP32_ISR_Servos.setupServo(1, 500, 2500);// x is pin 1
  y_servo = ESP32_ISR_Servos.setupServo(2, 500, 2500);// y is pin 2
  ESP32_ISR_Servos.setPosition(y_servo, initial_y); // set 20 if doesnt work
  ESP32_ISR_Servos.setPosition(x_servo, initial_x);

  Serial.begin(115200);
  Serial.setDebugOutput(true);
  Serial.println();

  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_3;
  config.ledc_timer = LEDC_TIMER_3;
  config.pin_d0 = 33;
  config.pin_d1 = 34;
  config.pin_d2 = 35;
  config.pin_d3 = 36;
  config.pin_d4 = 37;
  config.pin_d5 = 38;
  config.pin_d6 = 39;
  config.pin_d7 = 40;
  config.pin_xclk = 12;
  config.pin_pclk = 11;
  config.pin_vsync = 9;
  config.pin_href = 10;
  config.pin_sscb_sda = 8;
  config.pin_sscb_scl = 7;
  config.pin_pwdn = -1;
  config.pin_reset = 13;
  config.xclk_freq_hz = 20000000;
  config.frame_size = FRAMESIZE_QVGA;
  //config.pixel_format = PIXFORMAT_JPEG; // for streaming
  config.pixel_format = PIXFORMAT_RGB565; // for face detection/recognition
  config.grab_mode = CAMERA_GRAB_WHEN_EMPTY;
  config.fb_location = CAMERA_FB_IN_PSRAM;
  config.jpeg_quality = 12;
  config.fb_count = 1;
  
  // if PSRAM IC present, init with UXGA resolution and higher JPEG quality
  //                      for larger pre-allocated frame buffer.
  //  config.frame_size = FRAMESIZE_QVGA;
#if CONFIG_IDF_TARGET_ESP32S3
    config.fb_count = 2;
#endif

#if defined(CAMERA_MODEL_ESP_EYE)
  pinMode(13, INPUT_PULLUP);
  pinMode(14, INPUT_PULLUP);
#endif

  // camera init
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Total PSRAM: 0x%x", (int) ESP.getPsramSize());
    Serial.printf("Camera init failed with error 0x%x", err);
    return;
    
  }

  sensor_t * s = esp_camera_sensor_get();
  // initial sensors are flipped vertically and colors are a bit saturated
  if (s->id.PID == OV3660_PID) {
    s->set_vflip(s, 1); // flip it back
    s->set_brightness(s, 1); // up the brightness just a bit
    s->set_saturation(s, -2); // lower the saturation
  }
  // drop down frame size for higher initial frame rate
  if(config.pixel_format == PIXFORMAT_JPEG){
    s->set_framesize(s, FRAMESIZE_QVGA);
  }

#if defined(CAMERA_MODEL_M5STACK_WIDE) || defined(CAMERA_MODEL_M5STACK_ESP32CAM)
  s->set_vflip(s, 1);
  s->set_hmirror(s, 1);
#endif

#if defined(CAMERA_MODEL_ESP32S3_EYE)
  s->set_vflip(s, 1);

#endif

  WiFi.begin(ssid, password);
  WiFi.setSleep(false);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("WiFi connected");

  Serial.print("Camera Stream Ready! Go to: http://");
  Serial.print(WiFi.localIP());
  
  startWebServer();

}

int angle = 1;
int width = 240;
int height = 320;
int x_current_pos = initial_y;
int y_current_pos = initial_x;
void rotate(int x, int y) {
    if (x > (height/2) + 25) {
        x_current_pos -= angle; 
    }
    else if (x < (height / 2) - 25) {
        x_current_pos += angle;
    }

    if (y > (width / 2) + 25) { // opposite (needs to see how its built to determine)
        y_current_pos += (angle + 2); // maybe need to change this to the opposite
    }
    else if (y < (width / 2) - 25) {
        y_current_pos -= (angle + 2);
    }
    
    // add bounds to check above 90 degrees of below later
    if (y_current_pos > 180) {
      y_current_pos = 0;
    }
    else if (y_current_pos < 0) {
      y_current_pos = 180;
    }
    if (x_current_pos > 57) {
      x_current_pos = 57; // should work?
    }
    else if (x_current_pos < 18) {
      x_current_pos = 18;
    }
    unsigned int position_x = (unsigned int) x_current_pos;
    unsigned int position_y = (unsigned int) y_current_pos; 
    Serial.println(position_x);
    Serial.println(position_y);

    ESP32_ISR_Servos.setPosition(x_servo, position_y);
    ESP32_ISR_Servos.setPosition(y_servo, position_x);
    delay(400);
}

void rotate_man(int degree_x, int degree_y) {
  ESP32_ISR_Servos.setPosition(x_servo, degree_x);
  ESP32_ISR_Servos.setPosition(y_servo, degree_y);
  delay(100);
}

// Add the main program code into the continuous loop() function
void loop() {
  // lets say the person is in position x: 300 and y: 50
  //rotator.rotate(240, 50); // and thats how she works
}