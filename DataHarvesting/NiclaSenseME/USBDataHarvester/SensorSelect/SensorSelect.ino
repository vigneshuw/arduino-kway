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
char state;

// Send data as packed struct, ie. as the raw bytes
struct __attribute__((__packed__)) Data {
    // Timestamp
    float ts;
    // Acceleration
    float x;
    float y;
    float z;
    float w;  
} data;

// Sensors
// Accelerometer corrected
SensorXYZ accelerometer(SENSOR_ID_ACC);
SensorXYZ gyroscope(SENSOR_ID_GYRO);
SensorQuaternion quaternion(SENSOR_ID_RV);

void setup()
{
    nicla::begin();
    nicla::leds.begin();
    BHY2.begin();

    // Init the PacketSerial communication
    myPacketSerial.begin(250000);
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
        if (state == 'A') {
          data = { now, accelerometer.x(), accelerometer.y(), accelerometer.z(), 0.0};
        } else if (state == 'G') {
          data = { now, gyroscope.x(), gyroscope.y(), gyroscope.z(), 0.0};
        } else {
          data = { now, quaternion.x(), quaternion.y(), quaternion.z(), quaternion.w()};
        }

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
    case 'A':
        nicla::leds.setColor(green);
        // Init
        accelerometer.begin();
        accelerometer.configure(800.0, 0);
        delay(1000);
        // Set the previous state tracker
        state = 'A';
        capture = true;
        break;
    case 'G':
        nicla::leds.setColor(red);
        // Init
        gyroscope.begin();
        gyroscope.configure(800.0, 0);
        delay(1000);
        state = 'G';
        capture = true;
        break;
    case 'R':
        nicla::leds.setColor(blue);
        // Init
        quaternion.begin();
        quaternion.configure(800.0, 0);
        delay(1000);
        state = 'R';
        capture = true;
        break;
    case 'Q':
        nicla::leds.setColor(off);
        // End the respective sensor
        if (state == 'A') {
          accelerometer.end();
        } else if (state == 'G') {
          gyroscope.end();
        } else {
          quaternion.end();
        }
        state = 'Q';
        capture = false;
        break;

    default:
        break;
    }
}

