/* 
 * Library for creating a BLE Group for local exchange of data over BLE
 * among Gen3 Particle devices.
 */

#ifndef BleGroup_h
#define BleGroup_h

#include "Particle.h"
#include <deque>

#define MESSAGE_SIZE 244
#define EVENT_MAX 63
#define BLE_GROUP_MAX_PERIPHERALS 3

const BleUuid serviceUuid("7030fed9-88ac-43d7-b1ca-ba8c9b2fc755");
const BleUuid rxUuid("7de067f3-f3a1-4c81-a02f-98777e099f21");
const BleUuid txUuid("4350f719-9d57-40e6-9cb5-9922e1bae48a");

typedef void (*EventHandler)(const char *event_name, const char *data);

class BLE_Group
{
public:
  BLE_Group(uint16_t groupID);
  ~BLE_Group();

  void receive(const char *data, size_t len);
  virtual void subscribe(const char *event, EventHandler handler);
  virtual int publish(const char *event, const char *data) = 0;
  virtual int scan();
  virtual uint8_t devices_connected();
  virtual bool isCentral() = 0;

  class Subscriber
  {
  public:
    Subscriber(const char *event, EventHandler handler);
    const char* getEvent(void) {return _event;};
    EventHandler getHandler(void) {return _handler;};

  protected:
    const char *_event;
    EventHandler _handler;
  };

  class MessageQ
  {
  public:
    int encode(const char *event, const char *data);
    void store(const char *data, size_t len);
    void getEvent(char *event);
    void getData(char *data);
    char _data[MESSAGE_SIZE];
  };

protected:
  uint16_t _groupID;
  std::deque<Subscriber> _sub_q;
};

class BLE_Group_Central : public BLE_Group
{
public:
  BLE_Group_Central(uint16_t groupId);
  virtual int scan();
  virtual int publish(const char *event, const char *data);
  virtual uint8_t devices_connected();
  virtual bool isCentral() { return true; };

  class GroupPeripheral
  {
  public:
    GroupPeripheral() { in_use = false;};
    BlePeerDevice peer;
    bool in_use;
    BleCharacteristic _txCharacteristic;
    BleCharacteristic _rxCharacteristic;
  };

  GroupPeripheral peripherals[BLE_GROUP_MAX_PERIPHERALS];
};

class BLE_Group_Peripheral : public BLE_Group
{
public:
  BLE_Group_Peripheral(uint16_t groupId);
  virtual int publish(const char *event, const char *data);
  virtual bool isCentral() {return false;};

  bool connected;
private:
  BleCharacteristic _txCharacteristic, _rxCharacteristic;
};

#endif