/*
 * Copyright (C) 2021-2022 Huawei Device Co., Ltd.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "ohos_bt_spp.h"

#include <iostream>
#include <cstring>
#include <vector>

#include "ohos_bt_adapter_utils.h"
#include "bluetooth_socket.h"
#include "bluetooth_host.h"
#include "bluetooth_log.h"

#ifdef __cplusplus
extern "C" {
#endif

using namespace std;

namespace OHOS {
namespace Bluetooth {
struct ServerSocketWrapper {
    std::shared_ptr<SppServerSocket> serverSocket;
};

struct ClientSocketWrapper {
    std::shared_ptr<SppClientSocket> clientSocket;
};

using SppServerIterator = std::map<int, std::shared_ptr<ServerSocketWrapper>>::iterator;
using SppClientIterator = std::map<int, std::shared_ptr<ClientSocketWrapper>>::iterator;

static int g_ServerIncrease = 1;
static int g_ClientIncrease = 1;
static std::map<int, std::shared_ptr<ServerSocketWrapper>> g_SppServerMap;
static std::map<int, std::shared_ptr<ClientSocketWrapper>> g_SppClientMap;

#define SPP_SERVER_MAP g_SppServerMap
#define SPP_CLIENT_MAP g_SppClientMap

/**
 * @brief Creates an server listening socket based on the service record.
 *
 * @param socketPara The parameters to create a server socket.
 * @param name The service's name.
 * @param len The length of the service's name.
 * @return Returns a server ID, if create fail return {@link BT_SPP_INVALID_ID}.
 */
int SppServerCreate(BtCreateSocketPara *socketPara, const char *name, unsigned int len)
{
    HILOGI("SppServerCreate start!");
    if (!IS_BT_ENABLED()) {
        HILOGE("SppServerCreate fail,BR is not TURN_ON");
        return BT_SPP_INVALID_ID;
    }

    if (socketPara->socketType != OHOS_SPP_SOCKET_RFCOMM || strlen(name) != len ||
        strlen(socketPara->uuid.uuid) != socketPara->uuid.uuidLen) {
        HILOGI("SppServerCreate param invalid!");
        return BT_SPP_INVALID_ID;
    }

    string serverName(name);
    string tmpUuid(socketPara->uuid.uuid);
    UUID serverUuid(UUID::FromString(tmpUuid));
    std::shared_ptr<SppServerSocket> server = std::make_shared<SppServerSocket>(serverName, serverUuid,
        SppSocketType(socketPara->socketType), socketPara->isEncrypt);
    std::shared_ptr<ServerSocketWrapper> ServerWrap = std::make_shared<ServerSocketWrapper>();
        ServerWrap->serverSocket = server;
    int serverId = g_ServerIncrease++;
    SPP_SERVER_MAP.insert(std::make_pair(serverId, ServerWrap));
    HILOGI("SppServerCreate success, serverId: %{public}d", serverId);
    return serverId;
}

/**
 * @brief Waits for a remote device to connect to this server socket.
 *
 * This method return a client ID indicates a client socket
 * can be used to read data from and write data to remote device.
 *
 * @param serverId The relative ID used to identify the current server socket, obtain the value by calling
 * {@link SppServerCreate}.
 * @return Returns a client ID, if accept fail return {@link BT_SPP_INVALID_ID}.
 */
int SppServerAccept(int serverId)
{
    HILOGI("SppServerAccept start, serverId: %{public}d", serverId);
    SppServerIterator iter = SPP_SERVER_MAP.find(serverId);
    if (iter == SPP_SERVER_MAP.end()) {
        HILOGE("SppServerAccept serverId is not exist!");
        return BT_SPP_INVALID_ID;
    }

    std::shared_ptr<SppServerSocket> server = iter->second->serverSocket;
    if (server == nullptr) {
        HILOGE("SppServerAccept server is null!");
        return BT_SPP_INVALID_ID;
    }

    std::shared_ptr<SppClientSocket> client = server->Accept(0);
    if (client == nullptr) {
        HILOGE("SppServerAccept client is null!");
        return BT_SPP_INVALID_ID;
    }

    std::shared_ptr<ClientSocketWrapper> clientWrap = std::make_shared<ClientSocketWrapper>();
    clientWrap->clientSocket = client;
    int clientId = g_ClientIncrease++;
    SPP_CLIENT_MAP.insert(std::make_pair(clientId, clientWrap));
    HILOGI("SppServerAccept success, clientId: %{public}d", clientId);
    return clientId;
}

/**
 * @brief Disables an spp server socket and releases related resources.
 *
 * @param serverId The relative ID used to identify the current server socket, obtain the value by calling
 * {@link SppServerCreate}.
 * @return Returns the operation result status {@link BtStatus}.
 */
int SppServerClose(int serverId)
{
    HILOGI("SppServerClose, serverId: %{public}d", serverId);
    SppServerIterator iter = SPP_SERVER_MAP.find(serverId);
    if (iter == SPP_SERVER_MAP.end()) {
        HILOGE("SppServerClose serverId is not exist!");
        return OHOS_BT_STATUS_FAIL;
    }

    std::shared_ptr<SppServerSocket> server = iter->second->serverSocket;
    if (server == nullptr) {
        HILOGE("SppServerClose server is null!");
        return OHOS_BT_STATUS_FAIL;
    }
    server->Close();
    SPP_SERVER_MAP.erase(serverId);
    return OHOS_BT_STATUS_SUCCESS;
}

/**
 * @brief Connects to a remote device over the socket.
 *
 * @param socketPara The param to create a client socket and connect to a remote device.
 * @return Returns a client ID, if connect fail return {@link BT_SPP_INVALID_ID}.
 */
int SppConnect(BtCreateSocketPara *socketPara, const BdAddr *bdAddr)
{
    HILOGI("SppConnect start!");
    string strAddress;
    ConvertAddr(bdAddr->addr, strAddress);
    std::shared_ptr<BluetoothRemoteDevice> device = std::make_shared<BluetoothRemoteDevice>(strAddress, 0);
    string tmpUuid(socketPara->uuid.uuid);
    UUID serverUuid(UUID::FromString(tmpUuid));
    std::shared_ptr<SppClientSocket> client = std::make_shared<SppClientSocket>(*device, serverUuid,
        SppSocketType(socketPara->socketType), socketPara->isEncrypt);

    int result = client->Connect();
    if (result == OHOS_BT_STATUS_SUCCESS) {
        std::shared_ptr<ClientSocketWrapper> clientWrap =  std::make_shared<ClientSocketWrapper>();
        clientWrap->clientSocket = client;
        int clientId = g_ClientIncrease++;
        SPP_CLIENT_MAP.insert(std::make_pair(clientId, clientWrap));
        HILOGI("SppConnect success, clientId: %{public}d", clientId);
        return clientId;
    } else {
        HILOGE("SppConnect fail, result: %{public}d", result);
        return BT_SPP_INVALID_ID;
    }
}

/**
 * @brief Disables a connection and releases related resources.
 *
 * @param clientId The relative ID used to identify the current client socket.
 * @return Returns the operation result status {@link BtStatus}.
 */
int SppDisconnect(int clientId)
{
    HILOGI("SppDisconnect, clientId: %{public}d", clientId);
    SppClientIterator iter = SPP_CLIENT_MAP.find(clientId);
    if (iter == SPP_CLIENT_MAP.end()) {
        HILOGE("SppDisconnect clientId is not exist!");
        return OHOS_BT_STATUS_FAIL;
    }
    std::shared_ptr<SppClientSocket> client = iter->second->clientSocket;
    if (client == nullptr) {
        HILOGE("SppDisconnect client is null!");
        return OHOS_BT_STATUS_FAIL;
    }
    client->Close();
    SPP_CLIENT_MAP.erase(clientId);
    return OHOS_BT_STATUS_SUCCESS;
}

/**
 * @brief Spp get remote device's address.
 *
 * @param clientId The relative ID used to identify the current client socket.
 * @param remoteAddr Remote device's address, memory allocated by caller.
 * @return Returns the operation result status {@link BtStatus}.
 */
int SppGetRemoteAddr(int clientId, BdAddr *remoteAddr)
{
    HILOGI("SppGetRemoteAddr, clientId: %{public}d", clientId);
    SppClientIterator iter = SPP_CLIENT_MAP.find(clientId);
    if (iter == SPP_CLIENT_MAP.end()) {
        HILOGE("SppGetRemoteAddr clientId is not exist!");
        return OHOS_BT_STATUS_FAIL;
    }
    std::shared_ptr<SppClientSocket> client = iter->second->clientSocket;
    if (client == nullptr) {
        HILOGE("SppGetRemoteAddr client is null!");
        return OHOS_BT_STATUS_FAIL;
    }
    string tmpAddr = client->GetRemoteDevice().GetDeviceAddr();
    GetAddrFromString(tmpAddr, remoteAddr->addr);
    HILOGI("SppGetRemoteAddr, addr: %{public}02X:%{public}02X:*:*:*:%{public}02X",
        remoteAddr->addr[0], remoteAddr->addr[1], remoteAddr->addr[5]);
    return OHOS_BT_STATUS_SUCCESS;
}

/**
 * @brief Get the connection status of this socket.
 *
 * @param clientId The relative ID used to identify the current client socket.
 * @return Returns true is connected or false is not connected.
 */
bool IsSppConnected(int clientId)
{
    SppClientIterator iter = SPP_CLIENT_MAP.find(clientId);
    if (iter == SPP_CLIENT_MAP.end()) {
        HILOGE("IsSppConnected clientId is not exist, clientId: %{public}d", clientId);
        return false;
    }
    std::shared_ptr<SppClientSocket> client = iter->second->clientSocket;
    if (client == nullptr) {
        HILOGE("IsSppConnected client is null, clientId: %{public}d", clientId);
        return false;
    }
    bool isConnected = client->IsConnected();
    HILOGI("IsSppConnected, clientId: %{public}d, isConnected: %{public}d", clientId, isConnected);
    return isConnected;
}

/**
 * @brief Read data from socket.
 *
 * @param clientId The relative ID used to identify the current client socket.
 * @param buf Indicate the buffer which read in, memory allocated by caller.
 * @param bufLen Indicate the buffer length.
 * @return Returns the length greater than 0 as read the actual length.
 * Returns {@link BT_SPP_READ_SOCKET_CLOSED} if the socket is closed.
 * Returns {@link BT_SPP_READ_FAILED} if the operation failed.
 */
int SppRead(int clientId, char *buf, const unsigned int bufLen)
{
    SppClientIterator iter = SPP_CLIENT_MAP.find(clientId);
    if (iter == SPP_CLIENT_MAP.end()) {
        HILOGE("SppRead clientId is not exist!");
        return BT_SPP_READ_FAILED;
    }
    std::shared_ptr<SppClientSocket> client = iter->second->clientSocket;
    if (client == nullptr) {
        HILOGE("SppRead client is null!");
        return BT_SPP_READ_FAILED;
    }
    int readLen = client->GetInputStream().Read(buf, bufLen);
    HILOGI("SppRead ret, clientId: %{public}d, readLen: %{public}d", clientId, readLen);
    return readLen;
}

/**
 * @brief Client write data to socket.
 *
 * @param clientId The relative ID used to identify the current client socket.
 * @param data Indicate the data to be written.
 * @return Returns the actual write length.
 * Returns {@link BT_SPP_WRITE_FAILED} if the operation failed.
 */
int SppWrite(int clientId, const char *data, const unsigned int len)
{
    HILOGI("SppWrite start, clientId: %{public}d, len: %{public}d", clientId, len);
    SppClientIterator iter = SPP_CLIENT_MAP.find(clientId);
    if (iter == SPP_CLIENT_MAP.end()) {
        HILOGE("SppWrite clientId is not exist!");
        return OHOS_BT_STATUS_FAIL;
    }
    std::shared_ptr<SppClientSocket> client = iter->second->clientSocket;
    if (client == nullptr) {
        HILOGE("SppWrite client is null!");
        return OHOS_BT_STATUS_FAIL;
    }
    int writeLen = client->GetOutputStream().Write(data, len);
    HILOGI("SppWrite end, writeLen: %{public}d", writeLen);
    return writeLen;
}
}  // namespace Bluetooth
}  // namespace OHOS
#ifdef __cplusplus
}
#endif