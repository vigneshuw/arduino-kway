#include "Nicla_System.h"
#include "Arduino_BHY2.h"
#include <ArduinoBLE.h>
#include "BQ25120A.h"

#define UUID(val) ("19b10000-" val "-537e-4f6c-d104768a1214")

// Forward declaration
bool make_inference(float *);


/*
Processing parameters for accelertion sensor
*/
// Standardization values
float Xmin[3] = {-17925, -7158, -13527};
float Xmax[3] = {10019, 27096, 18462};
float diff[3];
// Thresholds
float threshold_max[3] = {0.95756331, 0.57578041, 0.76969089};
float threshold_min[3] = {0.33809659, 0.11072219, 0.23059389};
// Store the normalized acceleration
float normalized_accl[3];
// Bosch Sensortech
SensorXYZ accl(SENSOR_ID_ACC);


/*
BLE
*/
BLEDevice peripheral;
BLECharacteristic controlCharacteristic;
BLECharacteristic statusCharacteristic;
bool is_connected = false;


/*
Status Monitoring
*/
uint8_t is_monitoring = 0x00;
uint8_t is_fall = 0x00;
unsigned long previous_millis = millis();


void startSensors() {

  // Leds blink
  for (auto i = 0u; i < 4; i++) {
    nicla::leds.setColor(yellow);
    delay(25);
    nicla::leds.setColor(off);
    delay(25);
  }

  // Configue the sensor
  accl.begin();
  accl.configure(400, 0);
  delay(3000);

  // Leds blink
  for (auto i = 0u; i < 4; i++) {
    nicla::leds.setColor(green);
    delay(25);
    nicla::leds.setColor(off);
    delay(25);
  }
}


bool scanForPeripheral() {

  // Enable scanning the peripheral
  while(!is_connected) {

    // Start the scan
    BLE.scanForUuid(UUID("0000"));
    peripheral = BLE.available();

    if (peripheral) {
      if (peripheral.localName() == "Core2"){
        BLE.stopScan();

        // Enable connection to peripheral
        if(peripheral.connect()) {
          is_connected = true;
          startSensors();
        // Start sensors
        } else {
          return false;
        }
        // Discover Attributes
        if (!peripheral.discoverAttributes()) {
          peripheral.disconnect();
          is_connected = false;
          return false;
        } 
        return true;
      }
    }
    BLE.stopScan();
    delay(1000);

  }
}

bool connectToCharacteristics() {

  // All characteristics
  controlCharacteristic = peripheral.characteristic(UUID("0001"));
  statusCharacteristic = peripheral.characteristic(UUID("0002"));
  
  // Check characteristics
  if(!controlCharacteristic || !statusCharacteristic) {
    peripheral.disconnect();
    return false;
  } else if (!statusCharacteristic.canWrite()) {
    peripheral.disconnect();
    return false;
  } else if (!controlCharacteristic.canSubscribe()) {
    peripheral.disconnect();
    return false;
  }

  // Make the subscritpion
  controlCharacteristic.subscribe();
  return true;
}


void setup() {
  // Compute the difference
  for(int i = 0; i < 3; i++) {
    diff[i] = Xmax[i] - Xmin[i];
  }

  // Begin Nicla sense
  nicla::begin();
  nicla::leds.begin();
  // Charge
  nicla::enableCharge(100);
  BHY2.begin(NICLA_STANDALONE);

  /*
  Intialized BLE
  */
  if(!BLE.begin()) {
    while (1)
      ;
  }
  // Scan for the required device
  bool scanStatus = false;
  bool characteristicStatus = false;
  do {
    scanStatus = scanForPeripheral();
    if (scanStatus) characteristicStatus = connectToCharacteristics();
  } while (!(scanStatus && characteristicStatus));

  pinMode(LED_BUILTIN, OUTPUT);
  for (auto i = 0u; i < 10; i++) {
      nicla::leds.setColor(blue);
      delay(25);
      nicla::leds.setColor(off);
      delay(25);
  }
}

void loop() {

  BHY2.update();

  if (peripheral.connected()) {

    /*
    Monitoring of fall
    */
    if (is_monitoring == 1) {
      // get raw accl files
      float raw_accl[] = {accl.x(), accl.y(), accl.z()};
      // Normalize acceleration values
      for (int i = 0; i < 3; i++) {
        normalized_accl[i] = (raw_accl[i] - Xmin[i])/diff[i];
      };

      // Make inference for fall
      if(make_inference(normalized_accl)) {
        // Fall detected
        if ((millis() - previous_millis) >= 10000) {
          is_fall = 0x01;
          statusCharacteristic.writeValue((byte)is_fall);
          previous_millis = millis();
        }     
      }
    }

    /*
    Control Status
    */
    if (controlCharacteristic.valueUpdated()) {
      // Read the value
      byte value = 0;
      controlCharacteristic.readValue(value);
      is_monitoring = (uint8_t) value;

      // Indicate change in monitoring
      for (auto i = 0u; i < 2; i++) {
        nicla::leds.setColor(yellow);
        delay(25);
        nicla::leds.setColor(off);
        delay(25);
      }
    }

  } else {
    // Indicate
    // Leds blink
    for (auto i = 0u; i < 4; i++) {
      nicla::leds.setColor(red);
      delay(25);
      nicla::leds.setColor(off);
      delay(25);
    }

    // Refresh some values
    // Stop monitoring
    if (is_monitoring == 1) {
      is_monitoring = 0;
      accl.end();
    }
    is_fall = 0;
    is_connected = false;

    // Start scanning and connecting again
    bool scanStatus = false;
    bool characteristicStatus = false;
    do {
      // Scan and connect
      scanStatus = scanForPeripheral();
      if(scanStatus) characteristicStatus = connectToCharacteristics();
    } while (!(scanStatus && characteristicStatus));

  }

}

bool make_inference(float *data) {
  // Threshold condition
  if (((data[0] > threshold_max[0]) || (data[0] < threshold_min[0])) && ((data[1] > threshold_max[1]) || (data[1] < threshold_min[1]))) {
    return true;
  } else if (((data[1] > threshold_max[1]) || (data[1] < threshold_min[1])) && ((data[2] > threshold_max[2]) || (data[2] < threshold_min[2]))) {
    return true;
  } else if (((data[0] > threshold_max[0]) || (data[0] < threshold_min[0])) && ((data[2] > threshold_max[2]) || (data[2] < threshold_min[2]))) {
    return true;
  } else {
    return false;
  }

}


