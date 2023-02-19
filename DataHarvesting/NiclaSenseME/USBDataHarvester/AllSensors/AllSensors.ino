/*
COBS based data transfer over Serial.
1. Follows the Arduino example
2. Start/Stop the data transfer using Bluetooth
3. High speed data transfer over bluetooth
*/

#include <Nicla_System.h>
#include <Arduino_BHY2.h>
#include <PacketSerial.h>

// Serial packet for transfer
PacketSerial myPacketSerial;

auto capture { false };

// Send data as packed struct, ie. as the raw bytes
struct __attribute__((__packed__)) Data {
    // Timestamp
    float ts;
    // Acceleration
    float ax;
    float ay;
    float az;
    // Gyroscope
    float gx;
    float gy;
    float gz;
    // Quaternion
    float qx;
    float qy;
    float qz;
    float qw;    
};

// Sensors
// Accelerometer corrected
SensorXYZ accelerometer(SENSOR_ID_ACC);
// Gyroscope corrected
SensorXYZ gyroscope(SENSOR_ID_GYRO);
// Quaternion
SensorQuaternion quaternion(SENSOR_ID_RV);

void setup()
{
    nicla::begin();
    nicla::leds.begin();
    // Enable Device charging
    nicla::enableCharge(100);
    BHY2.begin();
    // Init
    accelerometer.begin();
    accelerometer.configure(400, 0);
    gyroscope.begin();
    gyroscope.configure(400, 0);
    quaternion.begin();
    quaternion.configure(400, 0);

    // Init the PacketSerial communication
    myPacketSerial.begin(460800);
    // Set the function for handling commands from the companion app
    myPacketSerial.setPacketHandler(&onPacketReceived);

    pinMode(LED_BUILTIN, OUTPUT);
    for (auto i = 0u; i < 10; i++) {
        nicla::leds.setColor(green);
        delay(25);
        nicla::leds.setColor(off);
        delay(25);
    }

}


void loop()
{
    // Update communication-channel and sensors
    myPacketSerial.update();
    BHY2.update();

    // Capture and send data as soon as we read it
    if (capture == true) {
        float now = micros() / 1000.0;

        // Collect data from all sensors
        Data data { now, accelerometer.x(), accelerometer.y(), accelerometer.z(), 
        gyroscope.x(), gyroscope.y(), gyroscope.z(), 
        quaternion.x(), quaternion.y(), quaternion.z(), quaternion.w()
        };

        constexpr size_t dataBufLen { sizeof(Data) };
        uint8_t dataBuf[dataBufLen] {};

        // Convert the Data struct to an array of bytes
        memcpy(dataBuf, reinterpret_cast<void*>(&data), dataBufLen);
        
        // Send data
        myPacketSerial.send(dataBuf, dataBufLen);
    }
}


// Parse commands from the companion app
void onPacketReceived(const uint8_t* buffer, size_t size)
{
    uint8_t tempBuffer[size];

    for (auto i = 0u; i < 2; i++) {
        nicla::leds.setColor(green);
        delay(25);
        nicla::leds.setColor(off);
        delay(25);
    }

    memcpy(tempBuffer, buffer, size);

    switch (tempBuffer[0]) {
    case 'R':
        nicla::leds.setColor(green);
        capture = true;
        break;
    case 'Q':
        nicla::leds.setColor(off);
        capture = false;
        break;

    default:
        break;
    }
}

