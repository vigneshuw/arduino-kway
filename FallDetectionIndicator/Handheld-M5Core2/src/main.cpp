#include <M5Core2.h>
#include <BLEDevice.h>
#include <BLE2902.h>


#define UUID(val) "19b10000-" val "-537e-4f6c-d104768a1214"


// BLE Service & Characteristics
BLEServer* pServer = NULL;
BLEService* pService = NULL;
BLECharacteristic *pControlCharacteristic = NULL;
BLECharacteristic *pStatusCharacteristic = NULL;

// Connection status 
static bool is_connected = false;
static uint8_t control_status = 0;
static uint8_t fall_status = 0;
// Reconnect requirements
static bool doConnect = false;

// Display status notification
std::string all_success_status = "BLE Initialization...";


// Callbacks
// Server Callback
class ServerCallback: public BLEServerCallbacks {

  void onConnect(BLEServer* pServer) {
    is_connected = true;
  }

  void onDisconnect(BLEServer* pServer) {
    is_connected = false;
    // Reset indicators
    control_status = 0;
    fall_status = 0;
    BLEDevice::startAdvertising();
  }

};

// Characteristic Callback
class statusCharacteristicCallback: public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic *pCharacteristic) {
    uint8_t value = *pCharacteristic->getValue().data();

    if (value == 1) {
      fall_status = true;
    } else {
      fall_status = false;
    }

  }
};


void lcdTopSection(std::string all_success_status, const char color) {
  // Top section
  M5.Lcd.drawFastHLine(0, 80, 320, WHITE);
  M5.Lcd.fillRect(0, 0, 320, 75, DARKGREY);
  if (color == 'r') {
    M5.Lcd.fillCircle(30, 37.5, 20, RED);
  } else {
    M5.Lcd.fillCircle(30, 37.5, 20, GREEN);
  }
  M5.Lcd.setTextColor(BLACK);
  M5.Lcd.setCursor(65, 33);
  M5.Lcd.println(all_success_status.c_str());
}


void fallAlert() {

  // Indicate fall
  int i = 0;
  while(i < 10) {
    M5.Lcd.setTextSize(2.5);
    M5.Lcd.fillRect(0, 85, 320, 60, RED);
    M5.Lcd.setCursor(75, 105);
    M5.Lcd.println("Fall detected!");
    M5.Spk.DingDong();
    delay(500);
    M5.Lcd.fillRect(0, 85, 320, 60, BLACK);
    delay(500);
    i++;
  }
  
}


void setup() {
  // M5Core2 
  // Lcd
  M5.begin();
  M5.Lcd.begin();
  M5.Lcd.setTextSize(2.5);
  M5.Lcd.setBrightness(100);
  // Top section update
  lcdTopSection(all_success_status, 'r');

  // Start - Stop - Shutdown
  M5.Lcd.setTextColor(WHITE);
  M5.Lcd.setCursor(20, 240);
  M5.Lcd.println("Enable Sensor");
  M5.Lcd.setCursor(270, 240);     
  M5.Lcd.println("Disable Sensor");

  // Serial
  Serial.begin(115200);
  Serial.println("Starting the BLE client!");

  /*
  BLE Initialization
  */
  // BLE Initialization
  BLEDevice::init("Core2");
  // Server & Service 
  pServer = BLEDevice::createServer();
  pServer->setCallbacks(new ServerCallback);
  pService = pServer->createService(UUID("0000"));
  // Characteristics
  pControlCharacteristic = pService->createCharacteristic(UUID("0001"), BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_WRITE | BLECharacteristic::PROPERTY_NOTIFY);
  pStatusCharacteristic = pService->createCharacteristic(UUID("0002"), BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_WRITE);
  pStatusCharacteristic->setCallbacks(new statusCharacteristicCallback);
  // Descriptor
  pControlCharacteristic->addDescriptor(new BLE2902());
  pStatusCharacteristic->addDescriptor(new BLE2902());

  // Start Service
  pService->start();

  // Start Advertising
  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(UUID("0000"));
  pAdvertising->setScanResponse(true);
  // Helps iphone connection issue
  pAdvertising->setMinPreferred(0x0);
  BLEDevice::startAdvertising();

  // Update banner
  all_success_status = "Initialized (Core2)";
  lcdTopSection(all_success_status, 'g');

  // Update Characteristics
  pControlCharacteristic->setValue(&control_status, 1);
  pStatusCharacteristic->setValue(&fall_status, 1);

  // Indicate start of monitoring
  M5.Lcd.fillCircle(49, 190, 25, RED);
  M5.Lcd.fillCircle(265, 190, 25, DARKGREEN);

}

void loop() {

  M5.update();
  
  /*
  Fall Alert
  */
  // Only when the client is connected
  if (is_connected) {
    // Indicate connection
    M5.Lcd.fillRoundRect(140, 180, 40, 40, 10, GREEN);

    // Just monitor for fall
    if (fall_status) {
      
      fallAlert();
      fall_status = false;
      pStatusCharacteristic->setValue(&fall_status, 1);
        
    }
  } else {
    // Indicate disconnect
    M5.Lcd.fillRoundRect(140, 180, 40, 40, 10, BLACK);
    // Indicate start of monitoring
    M5.Lcd.fillCircle(49, 190, 25, RED);
    M5.Lcd.fillCircle(265, 190, 25, DARKGREEN);
  }

  /*
  Control the sensor
  */
  // Start - Stop - Shutdown
  if (M5.BtnA.isPressed() && is_connected && control_status != 1) {
    // Update the DAQ Control
    control_status = 1;
    pControlCharacteristic->setValue(&control_status, 1);
    pControlCharacteristic->notify();

    // Indicate start of monitoring
    M5.Lcd.fillCircle(49, 190, 25, DARKGREEN);
    M5.Lcd.fillCircle(265, 190, 25, RED);
    
  }

  if (M5.BtnC.isPressed() && is_connected && control_status != 0) {
    // Update the DAQ Control
    control_status = 0;
    pControlCharacteristic->setValue(&control_status, 1);
    pControlCharacteristic->notify();

    // Indicate start of monitoring
    M5.Lcd.fillCircle(49, 190, 25, RED);
    M5.Lcd.fillCircle(265, 190, 25, DARKGREEN);
    
  }
  
}

