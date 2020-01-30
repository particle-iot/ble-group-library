#include "BLE_Group.h"
#include "string.h"


BLE_Group::BLE_Group(uint16_t groupID)
{
    _groupID = groupID;
}

void BLE_Group::subscribe(const char *event, EventHandler handler)
{
    Subscriber item = Subscriber(event, handler);
    _sub_q.push_back(item);
}

BLE_Group::Subscriber::Subscriber(const char *event, EventHandler handler)
{
    _event = event;
    _handler = handler;
}

int BLE_Group::MessageQ::encode(const char *event, const char *data)
{
    uint8_t ev_len, dat_len;

    if ( (ev_len = strlen(event)) > EVENT_MAX) {
        Log.warn("Event length longer than max value, truncating.");
        ev_len = EVENT_MAX;
    }
    // Maximum data size is maximum message size (set by BLE) minus 2
    // because we're using 2 bytes to store the length of event and
    // the length of the data field
    if ( (dat_len = strlen(data)) > (MESSAGE_SIZE - 2 - ev_len)) {
        Log.warn("Data length too long, truncating");
        dat_len = (MESSAGE_SIZE - 2 - ev_len);
    }
    // The data will look like this:
    // | Event Length | Event | ... | ... | Data length | Data | ... | ... |
    _data[0] = ev_len;
    memcpy(_data+1, event, ev_len);
    _data[1+ev_len] = dat_len;
    memcpy(_data+2+ev_len, data, dat_len);
    return ev_len+dat_len+2;
}

void BLE_Group::MessageQ::store(const char *data, size_t len)
{
    memcpy(_data, data, std::min(len, (size_t)MESSAGE_SIZE));
    if (_data[0] > EVENT_MAX) _data[0] = EVENT_MAX;
}

void BLE_Group::MessageQ::getEvent(char *event)
{
    memcpy(event, _data+1, _data[0]);
    event[(uint8_t)_data[0]] = '\0';
}

void BLE_Group::MessageQ::getData(char *ev_data)
{
    uint8_t ev_len, dat_len;
    ev_len = (uint8_t)_data[0];
    dat_len = (uint8_t)_data[1+ev_len];
    memcpy(ev_data, _data+2+ev_len, dat_len);
    ev_data[dat_len] = '\0';
    ev_data[MESSAGE_SIZE-1] = '\0';
}

void BLE_Group::receive(const char *data, size_t len)
{
    MessageQ new_item;
    char event[EVENT_MAX+1], ev_data[MESSAGE_SIZE-1];
    new_item.store(data, len);
    new_item.getEvent(event);
    new_item.getData(ev_data);
    //Log.info("Event: %s, Data: %s", event, ev_data);
    for(Subscriber sub : _sub_q)
    {
        size_t sub_length = strlen(sub.getEvent());
        if (strlen(event) >= sub_length && !strncmp(sub.getEvent(), event, sub_length))
        {
            sub.getHandler()(event, ev_data);
        }
    }
}

int BLE_Group::scan()
{
    Log.warn("Scan should be called as a Central");
    return -1;
}
uint8_t BLE_Group::devices_connected()
{
    Log.warn("Devices connected should be called as a Central");
    return 0;
}