/*
 * Project ble-group-diagnostics
 * Description: Several diagnostic tools for a local BLE Group
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
bool keep_scanning = true;

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

/*
 * throughputTest is a function exposed to the Particle Cloud to publish
 * packets over the local BLE Group as fast as possible, to test the
 * throughput to other devices
 * 
 * To start a test, call the ThroughputTest cloud function. The parameter
 * passed is the packet size that will be sent. 100 packets are sent.
 */
int throughputTest(String extra)
{
  if (!dev_configured) return -1;
  keep_scanning = false;
  size_t packet_size = std::atoi(extra);
  char packet[packet_size];
  memset(packet, 'A', packet_size-1);
  packet[packet_size-1] = '\0';
  group->publish("tx-start", "");
  for (size_t idx=0;idx<100;idx++)
  {
    group->publish("tx", packet);
  }
  group->publish("tx-end", "");
  keep_scanning = true;
  return 0;
}

/*
 * throughputFunc is the callback function to be used to subscribe
 * to the "tx" event, which is used for the throughput testing.
 * 
 * Once all the packets are received, it publishes to the Particle 
 * Cloud the results of the test (total bytes and kbps)
 */
unsigned long throughtput_timer;
uint32_t data_received;
void throughputFunc(const char *event, const char *data)
{
  if(strcmp(event, "tx-start") == 0)
  {
    throughtput_timer = millis();
    data_received = 0;
  } else if (strcmp(event,"tx") == 0)
  {
    data_received += strlen(data)+1;
  } else if (strcmp(event,"tx-end") == 0)
  {
    unsigned long total_time = millis() - throughtput_timer;
    char buf[100];
    snprintf(buf, sizeof(buf), "Bytes: %lu, Time: %lu, kbps: %lu", data_received, total_time, ( (data_received*8/1024)/(total_time/1000) ));
    Particle.publish("throughput", buf, PRIVATE);
  }
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

/*
 * Callback to monitor RSSI of other BLE devices with our service
 */
void onScan(const BleScanResult& scan, void* context)
{
  char buf[100];
#if defined(SYSTEM_VERSION_v300ALPHA1) && (SYSTEM_VERSION >= SYSTEM_VERSION_v300ALPHA1)
  snprintf(buf, sizeof(buf), "Address: %s, RSSI: %d", scan.address().toString().c_str(), scan.rssi());
#else
  snprintf(buf, sizeof(buf), "Address: %s, RSSI: %d", scan.address.toString().c_str(), scan.rssi);
#endif
  Particle.publish("Scan Result", buf, PRIVATE);
}

// setup() runs once, when the device is first turned on.
void setup() {
  Particle.function("SetPeripheral", setAsPeripheral);
  Particle.function("SetCentral", setAsCentral);
  Particle.function("ThroughputTest", throughputTest);
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
    group->subscribe("tx", throughputFunc);
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
    if (keep_scanning && group->devices_connected() < BLE_GROUP_MAX_PERIPHERALS && (millis() - scan_time) > 3000)
    {
      group->scan(onScan, NULL);
      scan_time = millis();
    }
  }
}