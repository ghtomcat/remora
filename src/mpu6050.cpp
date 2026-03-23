#include "mpu6050.h"

#if MPU6050_ENABLED

#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>

static Adafruit_MPU6050 mpu;
static bool initialized = false;
static bool hasReading = false;

// Latest sensor readings
static float accel_x, accel_y, accel_z;
static float gyro_x, gyro_y, gyro_z;
static float temperature;

void mpu6050Init() {
    Serial.println("MPU6050: initializing...");

    if (!mpu.begin(0x68)) {
        Serial.println("MPU6050: not found at 0x68, trying 0x69...");
        if (!mpu.begin(0x69)) {
            Serial.println("MPU6050: initialization failed!");
            return;
        }
    }

    Serial.println("MPU6050: found!");

    // Set accelerometer range (±2g, ±4g, ±8g, ±16g)
    mpu.setAccelerometerRange(MPU6050_RANGE_8_G);
    Serial.print("MPU6050: Accelerometer range set to ±");
    switch (mpu.getAccelerometerRange()) {
        case MPU6050_RANGE_2_G:  Serial.println("2G"); break;
        case MPU6050_RANGE_4_G:  Serial.println("4G"); break;
        case MPU6050_RANGE_8_G:  Serial.println("8G"); break;
        case MPU6050_RANGE_16_G: Serial.println("16G"); break;
    }

    // Set gyro range (±250°/s, ±500°/s, ±1000°/s, ±2000°/s)
    mpu.setGyroRange(MPU6050_RANGE_500_DEG);
    Serial.print("MPU6050: Gyro range set to ±");
    switch (mpu.getGyroRange()) {
        case MPU6050_RANGE_250_DEG:  Serial.println("250°/s"); break;
        case MPU6050_RANGE_500_DEG:  Serial.println("500°/s"); break;
        case MPU6050_RANGE_1000_DEG: Serial.println("1000°/s"); break;
        case MPU6050_RANGE_2000_DEG: Serial.println("2000°/s"); break;
    }

    // Set filter bandwidth
    mpu.setFilterBandwidth(MPU6050_BAND_21_HZ);
    Serial.print("MPU6050: Filter bandwidth set to ");
    switch (mpu.getFilterBandwidth()) {
        case MPU6050_BAND_260_HZ: Serial.println("260 Hz"); break;
        case MPU6050_BAND_184_HZ: Serial.println("184 Hz"); break;
        case MPU6050_BAND_94_HZ:  Serial.println("94 Hz"); break;
        case MPU6050_BAND_44_HZ:  Serial.println("44 Hz"); break;
        case MPU6050_BAND_21_HZ:  Serial.println("21 Hz"); break;
        case MPU6050_BAND_10_HZ:  Serial.println("10 Hz"); break;
        case MPU6050_BAND_5_HZ:   Serial.println("5 Hz"); break;
    }

    initialized = true;
    Serial.println("MPU6050: ready");
}

void mpu6050SetupMotionDetect() {
    if (!initialized) {
        Serial.println("MPU6050: not initialized, can't setup motion detect");
        return;
    }

    // Configure motion detection
    // Threshold: 2 = motion detected at ~0.25g (enough for bus movement)
    // Duration: 20ms minimum motion duration
    mpu.setMotionDetectionThreshold(2);
    mpu.setMotionDetectionDuration(20);

    // Enable motion interrupt on INT pin
    mpu.setInterruptPinLatch(true);   // Latch interrupt until read
    mpu.setInterruptPinPolarity(false); // Active low
    mpu.setMotionInterrupt(true);      // Enable motion interrupt

    Serial.println("MPU6050: motion detection configured");
    Serial.println("MPU6050: threshold=0.25g, duration=20ms");
}

bool mpu6050MotionDetected() {
    if (!initialized) return false;

    // Check motion interrupt status
    return mpu.getMotionInterruptStatus();
}

void mpu6050Read() {
    if (!initialized) {
        hasReading = false;
        return;
    }

    sensors_event_t accel, gyro, temp;
    mpu.getEvent(&accel, &gyro, &temp);

    // Store readings
    accel_x = accel.acceleration.x;
    accel_y = accel.acceleration.y;
    accel_z = accel.acceleration.z;

    gyro_x = gyro.gyro.x;
    gyro_y = gyro.gyro.y;
    gyro_z = gyro.gyro.z;

    temperature = temp.temperature;

    hasReading = true;
}

bool mpu6050HasReading() {
    return hasReading;
}

void mpu6050GetData(float &accelX, float &accelY, float &accelZ,
                     float &gyroX, float &gyroY, float &gyroZ,
                     float &temp) {
    accelX = accel_x;
    accelY = accel_y;
    accelZ = accel_z;
    gyroX = gyro_x;
    gyroY = gyro_y;
    gyroZ = gyro_z;
    temp = temperature;
}

void mpu6050Print() {
    if (!hasReading) {
        Serial.println("MPU6050: no reading");
        return;
    }

    Serial.print("MPU6050: Accel(m/s²) X=");
    Serial.print(accel_x, 2);
    Serial.print(" Y=");
    Serial.print(accel_y, 2);
    Serial.print(" Z=");
    Serial.print(accel_z, 2);

    Serial.print(" Gyro(°/s) X=");
    Serial.print(gyro_x, 2);
    Serial.print(" Y=");
    Serial.print(gyro_y, 2);
    Serial.print(" Z=");
    Serial.print(gyro_z, 2);

    Serial.print(" Temp=");
    Serial.print(temperature, 1);
    Serial.println("°C");
}

#endif // MPU6050_ENABLED
