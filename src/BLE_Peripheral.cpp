#include "BLE_Group.h"

static void onDataReceived(const uint8_t *data, size_t len, const BlePeerDevice& peer, void* context)
{
    BLE_Group_Peripheral *ctx = (BLE_Group_Peripheral *)context;
    ctx->receive((const char *)data, len);
}

static void onPeriphConnected(const BlePeerDevice& peer, void* context)
{
    BLE_Group_Peripheral *ctx = (BLE_Group_Peripheral *)context;
    ctx->connected = true;
    Log.info("Connected to Central");
    if (ctx->_onConnectHandler != nullptr)
    {
        ctx->_onConnectHandler(peer, ctx->_onConnectContext);
    }
    BLE.stopAdvertising();
}
static void onPeriphDisconnected(const BlePeerDevice& peer, void* context)
{
    BLE_Group_Peripheral *ctx = (BLE_Group_Peripheral *)context;
    ctx->connected = false;
    Log.info("Disconnected from Central");
    if (ctx->_onDisconnectHandler != nullptr)
    {
        ctx->_onDisconnectHandler(peer, ctx->_onDisconnectContext);
    }
    BLE.advertise();
}

BLE_Group_Peripheral::BLE_Group_Peripheral(uint32_t groupId): BLE_Group(groupId)
{
    _txCharacteristic = BleCharacteristic("tx", BleCharacteristicProperty::INDICATE,
        txUuid, serviceUuid);
    _rxCharacteristic = BleCharacteristic("rx", BleCharacteristicProperty::WRITE,
        rxUuid, serviceUuid, onDataReceived, this);

    BLE.addCharacteristic(_txCharacteristic);
    BLE.addCharacteristic(_rxCharacteristic);

    BleAdvertisingData data;
    data.appendServiceUUID(serviceUuid);

    uint8_t custom_data[] = { 0x62, 0x06, 0, 0, 0, 0 };
    memcpy(custom_data+2, &_groupID, 4);
    data.appendCustomData(custom_data, sizeof(custom_data), false);
    BLE.advertise(&data);
    BLE.onConnected(onPeriphConnected, this);
    BLE.onDisconnected(onPeriphDisconnected, this);
}

/*
 * Publish a message to the group.
 */
int BLE_Group_Peripheral::publish(const char *event, const char *data)
{
    MessageQ new_item;
    int ret;
    // Create the packet that can be sent over BLE
    int packet_len = new_item.encode(event, data);
    // Indicate to central
    ret = _txCharacteristic.setValue((const uint8_t *)new_item._data, (size_t)std::min(packet_len, MESSAGE_SIZE));
    if (ret != packet_len) { return ret;}
    return 0;
}