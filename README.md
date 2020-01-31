# BLE_Group library

A library that works with Particle Gen3 devices to create a local group over Bluetooth Low Energy (BLE) to publish/subscribe messages.

The library works by connecting the devices over BLE. One of the devices in the group acts in the Central role, while the others act in the Peripheral role.

Data is exchanged one to one from the Central to each Peripheral. However, when data is sent from a Peripheral to the Central, it both "consumes" the data locally, as well as forwarding the data to all the other Peripherals that it is connected with.

## Typical Usage

You will need 2 or more devices. One will act as the Central, and the other(s) as Peripheral.

On the Central device:

```
#include "BLE_Group.h"
BLE_Group *group;

void callbackFunc(const char *event, const char *data)
{
  Log.info("Event: %s, Data: %s", event, data);
}

void setup() {
  group = new BLE_Group_Central(1); // The parameter is the groupID
  group->subscribe("test", callbackFunc);
}

void loop() {
  group->publish("test-central", "Some data");
}
```

On the peripheral device:

```
#include "BLE_Group.h"
BLE_Group *group;

void callbackFunc(const char *event, const char *data)
{
  Log.info("Event: %s, Data: %s", event, data);
}

void setup() {
  group = new BLE_Group_Peripheral(1); // The parameter is the groupID
  group->subscribe("test", callbackFunc);
}

void loop() {
  group->publish("test-periph", "Some Data");
}
```

It is also possible to use the same application on the Central and Peripheral devices, and use configuration variables stored in EEPROM to decide which is which. The examples provided with the library show how to do that.

## Examples

* __Diagnostics:__ Cloud-started throughput test, as well as reporting of group health by publishing Connect/Disconnect events
* __Heartbeat:__ Each device in the group will publish a heartbeat once per minute. The other devices will all receive it and use Particle.publish to send to the Particle cloud
* __Ping-pong:__ For just 2 devices (one Central and one Peripheral), send a number of events with small data payload back and forth and publish the total amount of time it took, to test message latency.
