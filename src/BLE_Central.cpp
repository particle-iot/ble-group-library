#include "BLE_Group.h"

#define SCAN_RESULT_MAX 30

BleScanResult scanResults[SCAN_RESULT_MAX];

static void onDisconnected(const BlePeerDevice &peer, void *context)
{
    BLE_Group_Central *ctx = (BLE_Group_Central *)context;
    Log.info("BLE Device disconnected: %02X:%02X:%02X:%02X:%02X:%02X", peer.address()[0],
             peer.address()[1], peer.address()[2], peer.address()[3], peer.address()[4], peer.address()[5]);
    for (uint8_t idx = 0; idx < BLE_GROUP_MAX_PERIPHERALS; idx++)
    {
        if (ctx->peripherals[idx].in_use && ctx->peripherals[idx].peer.address() == peer.address())
        {
            ctx->peripherals[idx].in_use = false;
        }
    }
    if (ctx->_onDisconnectHandler != nullptr)
    {
        ctx->_onDisconnectHandler(peer, ctx->_onDisconnectContext);
    }
}

static void onDataReceived(const uint8_t *data, size_t len, const BlePeerDevice& peer, void* context)
{
    BLE_Group_Central *ctx = (BLE_Group_Central *)context;
    ctx->receive((const char *)data, len);

    for (uint8_t idx = 0; idx < BLE_GROUP_MAX_PERIPHERALS; idx++)
    {
        if (ctx->peripherals[idx].in_use && !(ctx->peripherals[idx].peer.address() == peer.address()) )
        {
            ctx->peripherals[idx]._rxCharacteristic.setValue(data, len);
        }
    }
}

BLE_Group_Central::BLE_Group_Central(uint32_t groupId) : BLE_Group(groupId)
{
    BLE.onDisconnected(onDisconnected, this);
}

int BLE_Group_Central::scan(ScanEvent handler, void* context)
{
    int count = BLE.scan(scanResults, SCAN_RESULT_MAX);

    for (int ii = 0; ii < count; ii++)
    {
        BleUuid foundService;
        uint8_t buf[6];
        size_t len;

#if defined(SYSTEM_VERSION_v300ALPHA1) && (SYSTEM_VERSION >= SYSTEM_VERSION_v300ALPHA1)
        len = scanResults[ii].advertisingData().serviceUUID(&foundService, 1);
#else
        len = scanResults[ii].advertisingData.serviceUUID(&foundService, 1);
#endif
        if (len > 0 && foundService == serviceUuid)
        {
#if defined(SYSTEM_VERSION_v300ALPHA1) && (SYSTEM_VERSION >= SYSTEM_VERSION_v300ALPHA1)
            Log.info("Found our service with RSSI: %d", scanResults[ii].rssi());
#else
            Log.info("Found our service with RSSI: %d", scanResults[ii].rssi);
#endif
            if(handler != NULL)
            {
                handler(scanResults[ii], context);
            }
#if defined(SYSTEM_VERSION_v300ALPHA1) && (SYSTEM_VERSION >= SYSTEM_VERSION_v300ALPHA1)
            len = scanResults[ii].advertisingData().customData(buf, 6);
#else
            len = scanResults[ii].advertisingData.customData(buf, 6);
#endif
            if (len > 0 && memcmp(buf+2, &_groupID, 4) == 0)
            {
                bool connected = false;
                for (size_t kk = 0; kk < BLE_GROUP_MAX_PERIPHERALS; kk++)
                {
#if defined(SYSTEM_VERSION_v300ALPHA1) && (SYSTEM_VERSION >= SYSTEM_VERSION_v300ALPHA1)
                    if (peripherals[kk].in_use && peripherals[kk].peer.address() == scanResults[ii].address())
#else
                    if (peripherals[kk].in_use && peripherals[kk].peer.address() == scanResults[ii].address)
#endif
                    {
                        connected = true;
                        break;
                    }
                    if (!connected)
                    {
                        for (size_t hh = 0; hh < BLE_GROUP_MAX_PERIPHERALS; hh++)
                        {
                            if (!peripherals[hh].in_use)
                            {
#if defined(SYSTEM_VERSION_v300ALPHA1) && (SYSTEM_VERSION >= SYSTEM_VERSION_v300ALPHA1)
                                peripherals[hh].peer = BLE.connect(scanResults[ii].address());
#else
                                peripherals[hh].peer = BLE.connect(scanResults[ii].address);
#endif
                                if (peripherals[hh].peer.connected())
                                {
                                    Log.info("successfully connected!");
                                    if (_onConnectHandler != nullptr)
                                    {
                                        _onConnectHandler(peripherals[hh].peer, _onConnectContext);
                                    }
                                    connected = true;
                                    peripherals[hh].in_use = true;
                                    peripherals[hh].peer.getCharacteristicByUUID(peripherals[hh]._txCharacteristic, txUuid);
                                    peripherals[hh]._txCharacteristic.onDataReceived(onDataReceived, this);
                                    peripherals[hh].peer.getCharacteristicByUUID(peripherals[hh]._rxCharacteristic, rxUuid);
                                }
                                break;
                            }
                        }
                    }
                }
            }
            else
            {
                Log.info("Found a device but has another Group ID: %lu, our ID: %lu", *(uint32_t *)(buf+2), _groupID);
            }
        }
    }
    return 0;
}

int BLE_Group_Central::scan()
{
    return this->scan(NULL, NULL);
}

uint8_t BLE_Group_Central::devices_connected()
{
    uint8_t count = 0;
    for (uint8_t idx = 0; idx < BLE_GROUP_MAX_PERIPHERALS; idx++)
    {
        if (peripherals[idx].in_use)
            count++;
    }
    return count;
}

int BLE_Group_Central::publish(const char *event, const char *data)
{
    MessageQ new_item;
    int ret = 0;
    int intermediate;
    int packet_len = new_item.encode(event, data);
    for (uint8_t idx = 0; idx < BLE_GROUP_MAX_PERIPHERALS; idx++)
    {
        if (peripherals[idx].in_use)
        {
            intermediate = peripherals[idx]._rxCharacteristic.setValue((const uint8_t *)new_item._data, (size_t)std::min(packet_len,MESSAGE_SIZE));
            if (intermediate < 0) ret = intermediate;
        }
    }
    return ret;
}