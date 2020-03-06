/*
 * Project ble-group-heart-beat-example
 * Description: Create local BLE communication among multiple devices and monitor long term connectivity
 * Author: Mariano Goluboff
 * Date: January 24th, 2020
 * 
 * Instructions:
 * - Deploy firmware to several Gen3 devices (Argon or Boron)
 * - Use the Particle Console to run SetCentral on one device and SetPeripheral on the others
 *   all with the same numerical input (e.g. 5)
 * - Reboot all devices
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
  uint32_t group_number;
};

bool dev_configured;

unsigned long heartbeat_time, scan_time;

// Set the device flash configuration to peripheral.
// It is applied at the next reboot.
int setAsPeripheral(String extra)
{
  bleConfig myConfig = { 0, (uint32_t)std::atoi(extra)};
  EEPROM.put(eeprom_address, myConfig);
  return 0;
}

// Set the device flash configuration to central.
// It is applied at the next reboot
int setAsCentral(String extra)
{
  bleConfig myConfig = { 1, (uint32_t)std::atoi(extra)};
  EEPROM.put(eeprom_address, myConfig);
  return 0;
}

void kickFunc(const char *event, const char *data)
{
  Particle.publish("kick-received", data, PRIVATE);
}

/*
 * Callback functions to monitor connection/disconnection from the
 * local BLE Group.
 */
void onConnect(const BlePeerDevice& peer, void* context)
{
  Particle.publish("Connected", peer.address().toString(), PRIVATE);
}
void onDisconnect(const BlePeerDevice& peer, void* context)
{
  Particle.publish("Disconnected", peer.address().toString(), PRIVATE);
}


// setup() runs once, when the device is first turned on.
void setup() {
  Particle.function("SetPeripheral", setAsPeripheral);
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
    group->subscribe("kick", kickFunc);
    group->onConnect(onConnect, NULL);
    group->onDisconnect(onDisconnect, NULL);
  }
  scan_time = 0;
  heartbeat_time = 0;
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
    // Scan for more devices to build the group if we're the Central device
    // and we haven't hit the maximum number of devices yet.
    if (group->devices_connected() < BLE_GROUP_MAX_PERIPHERALS && (millis() - scan_time) > 3000)
    {
      group->scan();
      scan_time = millis();
    }
  }
  if(dev_configured && (millis() - heartbeat_time) > 60000 )
  {
    group->publish("kick", System.deviceID());
    heartbeat_time = millis();
  }
}