#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include <stdlib.h>

BLEServer* pServer = NULL;
BLECharacteristic* pCharacteristic = NULL;
bool deviceConnected = false;
bool oldDeviceConnected = false;
unsigned long previousMillis = 0;
const long interval = 1000;

// TODO: add new global variables for your sensor readings and processed data
// HC-SR04 sensor pins
const int triggerPin = 3;  // Trigger pin
const int echoPin = 2;    // Echo pin
const int numReadings = 10; // Number of readings for moving average filter

float readings[numReadings];  // Array to store sensor readings
int readIndex = 0;            // Index for the current reading
float total = 0;              // Sum of readings
float average = 0;            // Filtered value

// TODO: Change the UUID to your own (any specific one works, but make sure they're different from others'). You can generate one here: https://www.uuidgenerator.net/
#define SERVICE_UUID        "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define CHARACTERISTIC_UUID "beb5483e-36e1-4688-b7f5-ea07361b26a8"

class MyServerCallbacks : public BLEServerCallbacks {
    void onConnect(BLEServer* pServer) {
        deviceConnected = true;
    };

    void onDisconnect(BLEServer* pServer) {
        deviceConnected = false;
    }
};

// TODO: add DSP algorithm functions here
// Moving Average Filter to denoise sensor data
float movingAverage(float newReading) {
    total = total - readings[readIndex];    // Subtract the last reading
    readings[readIndex] = newReading;       // Add the new reading
    total = total + readings[readIndex];    // Add the new reading to the total
    readIndex = (readIndex + 1) % numReadings;  // Move to the next position in the array
    average = total / numReadings;          // Calculate the average
    return average;
}

void setup() {
    Serial.begin(115200);
    Serial.println("Starting BLE work!");

    // Initialize sensor readings array
    for (int i = 0; i < numReadings; i++) {
        readings[i] = 0;  // Initialize all readings to zero
    }
    // Set up HC-SR04 pins
    pinMode(triggerPin, OUTPUT);
    pinMode(echoPin, INPUT);

    // Initialize BLE
    // TODO: name your device to avoid conflictions
    BLEDevice::init("XIAO_ESP32S3_Katherine");
    pServer = BLEDevice::createServer();
    pServer->setCallbacks(new MyServerCallbacks());
    BLEService *pService = pServer->createService(SERVICE_UUID);
    pCharacteristic = pService->createCharacteristic(
        CHARACTERISTIC_UUID,
        BLECharacteristic::PROPERTY_READ |
        BLECharacteristic::PROPERTY_WRITE |
        BLECharacteristic::PROPERTY_NOTIFY
    );
    pCharacteristic->addDescriptor(new BLE2902());
    pCharacteristic->setValue("Hello World");
    pService->start();

    BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
    pAdvertising->addServiceUUID(SERVICE_UUID);
    pAdvertising->setScanResponse(true);
    pAdvertising->setMinPreferred(0x06);
    pAdvertising->setMinPreferred(0x12);
    BLEDevice::startAdvertising();
    Serial.println("Characteristic defined! Now you can read it in your phone!");
}

float getDistance() {
    // Send a pulse to trigger the sensor
    digitalWrite(triggerPin, LOW);
    delayMicroseconds(2);
    digitalWrite(triggerPin, HIGH);
    delayMicroseconds(10);
    digitalWrite(triggerPin, LOW);

    // Measure the duration of the pulse on the Echo pin
    long duration = pulseIn(echoPin, HIGH);

    // Calculate the distance in cm (duration is in microseconds)
    // Speed of sound is approximately 343 meters per second or 0.0343 cm/us
    float distance = duration * 0.0343 / 2;  // Divide by 2 to get the round-trip distance
    return distance;
}

void loop() {
    // TODO: add codes for handling your sensor readings (analogRead, etc.)
    // Apply moving average filter to smooth the data
    // Read distance from HC-SR04
    float distance = getDistance();
    float denoisedDistance = movingAverage(distance);

    // Print both raw and denoised readings
    Serial.print("Raw Distance: ");
    Serial.print(distance);
    Serial.print(" cm\tDenoised Distance: ");
    Serial.println(denoisedDistance);

    // TODO: use your defined DSP algorithm to process the readings
    // Only send the data if the denoised distance is less than 30 cm
    if (denoisedDistance < 30.0) {
        unsigned long currentMillis = millis();
        if (currentMillis - previousMillis >= interval) {
            // Convert the denoised distance to a string and send it over BLE
            String data = String(denoisedDistance, 2);  // Send the distance with 2 decimal places
            pCharacteristic->setValue(data.c_str());
            pCharacteristic->notify();
            //Serial.print("Notify value: ");
            //Serial.println(data);
            previousMillis = currentMillis;
        }
    }
    
    // Disconnecting logic (if BLE connection is lost)
    if (!deviceConnected && oldDeviceConnected) {
        delay(500);  // give the Bluetooth stack a chance to get things ready
        pServer->startAdvertising();  // Start advertising again
        Serial.println("Start advertising");
        oldDeviceConnected = deviceConnected;
    }

    // Connecting logic (when BLE connection is established)
    if (deviceConnected && !oldDeviceConnected) {
        // Do stuff here when connecting
        oldDeviceConnected = deviceConnected;
    }

    delay(1000);
}