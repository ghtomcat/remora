#pragma once

#include <Arduino.h>

#if MPU6050_ENABLED

#define MPU6050_INT_PIN WB_IO1  // Motion interrupt pin (IO1 = P0.17)

void mpu6050Init();
void mpu6050SetupMotionDetect();  // Configure motion detection interrupt
void mpu6050Read();
bool mpu6050HasReading();
void mpu6050GetData(float &accelX, float &accelY, float &accelZ,
                     float &gyroX, float &gyroY, float &gyroZ,
                     float &temp);
void mpu6050Print();
bool mpu6050MotionDetected();  // Check if motion interrupt triggered

#endif // MPU6050_ENABLED
