/*
 * Project ble-group-ping-pong-example
 * Description: Create local BLE communication between two devices and run ping pong testing
 * Author: Mariano Goluboff
 * Date: January 24th, 2020
 * 
 * Instructions:
 * - Deploy firmware to two Gen3 devices (Argon or Boron)
 * - Use the Particle Console to run SetPeripheral one one device and SetCentral on the other,
 *   both with the same numerical input (e.g. 5)
 * - Reboot both devices
 * - Run PingPong on one of the devices, with a value of 10 or 100
 */
#include "BLE_Group.h"
#include "Particle.h"
#include <string>

SYSTEM_THREAD(ENABLED);

SerialLogHandler logHandler;

BLE_Group *group;

int eeprom_address = 0;
struct bleConfig {
  uint8_t is_central;
  uint16_t group_number;
};

bool dev_configured;
bool keep_scanning = true;

unsigned long start_time;

// Set the device flash configuration to peripheral.
// It is applied at the next reboot.
int setAsPeripheral(String extra)
{
  bleConfig myConfig = { 0, (uint16_t)std::atoi(extra)};
  EEPROM.put(eeprom_address, myConfig);
  return 0;
}

// Set the device flash configuration to central.
// It is applied at the next reboot
int setAsCentral(String extra)
{
  bleConfig myConfig = { 1, (uint16_t)std::atoi(extra)};
  EEPROM.put(eeprom_address, myConfig);
  return 0;
}

// Start a ping-pong test for the number of round trips
// that is pased as argument
int startTest(String extra)
{
  start_time = millis();
  return group->publish("ping", extra);
}

// Send a pong when we receive a ping
void pingFunc(const char *event, const char *data)
{
  group->publish("pong", data);
}

// Receive the pong response and process it.
void pongFunc(const char *event, const char *data)
{
  int count = std::atoi(data);
  if (count > 0)
  {
    // if we're not yet to 0 in the count, send a ping having reduced by 1
    char buf[10];
    snprintf(buf, sizeof(buf), "%d", count - 1);
    group->publish("ping", buf);
  }
  else
  {
    // Get the end time and publish it
    unsigned long end_time = millis() - start_time;
    char buf[30];
    snprintf(buf, sizeof(buf), "Total time: %lu", end_time);
    Particle.publish("PingPong", buf, PRIVATE);
  }
}

String isConnected;

// setup() runs once, when the device is first turned on.
void setup() {
  Particle.function("SetPeripheral", setAsPeripheral);
  Particle.function("PingPong", startTest);
  Particle.function("SetCentral", setAsCentral);
  bleConfig myConfig;
  EEPROM.get(eeprom_address, myConfig);
  if (myConfig.is_central == 0xFF)
  {
    dev_configured = false;
  }
  else if (myConfig.is_central == 1)
  {
    group = new BLE_Group_Central(myConfig.group_number);
    dev_configured = true;
  }
  else if (myConfig.is_central == 0)
  {
    group = new BLE_Group_Peripheral(myConfig.group_number);
    dev_configured = true;
  }
  if (dev_configured)
  {
    group->subscribe("ping", pingFunc);
    group->subscribe("pong", pongFunc);
  }
  // Speed up transmission (this works for Particle devices talking to Particle devices
  // but can break communications to iPhones as they have higher latencies)
  BLE.setPPCP(12, 12, 0, 200);
  // Increase transmit power to max
  BLE.setTxPower(8);
}

// loop() runs over and over again, as quickly as it can execute.
void loop() {
  if (dev_configured && group->isCentral())
  {
    // Since this is point to point communications, stop scanning when we have a connection
    keep_scanning = (group->devices_connected() > 0) ? false : true;
    // Scan for more devices to build the group if we're the Central device
    // and we haven't hit the maximum number of devices yet.
    if (keep_scanning && group->devices_connected() < BLE_GROUP_MAX_PERIPHERALS)
    {
      group->scan();
    }
  }
}