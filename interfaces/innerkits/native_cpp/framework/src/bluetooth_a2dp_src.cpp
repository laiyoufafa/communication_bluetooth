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

#include "bluetooth_a2dp_src.h"
#include "bluetooth_a2dp_codec.h"
#include "bluetooth_a2dp_src_proxy.h"
#include "bluetooth_a2dp_src_observer_stub.h"
#include "bluetooth_host_proxy.h"
#include "bluetooth_observer_list.h"
#include "raw_address.h"
#include "bluetooth_def.h"
#include "bluetooth_host.h"

#include "bluetooth_log.h"
#include "iservice_registry.h"
#include "system_ability_definition.h"

namespace OHOS {
namespace Bluetooth {
using namespace bluetooth;

struct A2dpSource::impl {
    impl();
    ~impl();

    BluetoothObserverList<A2dpSourceObserver> observers_;
    sptr<IBluetoothA2dpSrc> proxy_ = nullptr;
    class BluetoothA2dpSourceObserverImp;
    sptr<BluetoothA2dpSourceObserverImp> observerImp_ = nullptr;
    class BluetoothA2dpSourceDeathRecipient;
    sptr<BluetoothA2dpSourceDeathRecipient> deathRecipient_ = nullptr;

private:
    void GetProxy();
};

class A2dpSource::impl::BluetoothA2dpSourceObserverImp : public BluetoothA2dpSrcObserverStub {
public:
    BluetoothA2dpSourceObserverImp(A2dpSource::impl &a2dpSource) : a2dpSource_(a2dpSource) {};
    ~BluetoothA2dpSourceObserverImp() override{};

    void Register(std::shared_ptr<A2dpSourceObserver> &observer)
    {
        HILOGD("%{public}s start.", __func__);
        a2dpSource_.observers_.Register(observer);
    }

    void Deregister(std::shared_ptr<A2dpSourceObserver> &observer)
    {
        HILOGD("%{public}s start.", __func__);
        a2dpSource_.observers_.Deregister(observer);
    }

    void OnConnectionStateChanged(const RawAddress &device, int state) override
    {
        HILOGD("%{public}s start.", __func__);
        a2dpSource_.observers_.ForEach([device, state](std::shared_ptr<A2dpSourceObserver> observer) {
            observer->OnConnectionStateChanged(BluetoothRemoteDevice(device.GetAddress(), 0), state);
        });
    }

    void OnPlayingStatusChanged(const RawAddress &device, int playingState, int error) override
    {
        HILOGD("%{public}s start.", __func__);
        a2dpSource_.observers_.ForEach([device, playingState, error](std::shared_ptr<A2dpSourceObserver> observer) {
            observer->OnPlayingStatusChanged(BluetoothRemoteDevice(device.GetAddress(), 0), playingState, error);
        });
    }

    void OnConfigurationChanged(const RawAddress &device, const BluetoothA2dpCodecInfo &info, int error) override
    {
        HILOGD("%{public}s start.", __func__);
        a2dpSource_.observers_.ForEach([device, info, error](std::shared_ptr<A2dpSourceObserver> observer) {
            A2dpCodecInfo codecInfo;

            codecInfo.bitsPerSample = info.bitsPerSample;
            codecInfo.channelMode = info.channelMode;
            codecInfo.codecPriority = info.codecPriority;
            codecInfo.codecType = info.codecType;
            codecInfo.sampleRate = info.sampleRate;

            observer->OnConfigurationChanged(BluetoothRemoteDevice(device.GetAddress(), 0), codecInfo, error);
        });
    };

private:
    A2dpSource::impl &a2dpSource_;
    BLUETOOTH_DISALLOW_COPY_AND_ASSIGN(BluetoothA2dpSourceObserverImp);
};

class A2dpSource::impl::BluetoothA2dpSourceDeathRecipient final : public IRemoteObject::DeathRecipient {
public:
    BluetoothA2dpSourceDeathRecipient(A2dpSource::impl &a2dpSrcDeath) : a2dpSrcDeath_(a2dpSrcDeath)
    {};
    ~BluetoothA2dpSourceDeathRecipient() final = default;
    BLUETOOTH_DISALLOW_COPY_AND_ASSIGN(BluetoothA2dpSourceDeathRecipient);

    void OnRemoteDied(const wptr<IRemoteObject> &remote) final
    {
        HILOGI("A2dpSource::impl::BluetoothA2dpSourceDeathRecipient::OnRemoteDied starts");
        a2dpSrcDeath_.proxy_->AsObject()->RemoveDeathRecipient(a2dpSrcDeath_.deathRecipient_);
        a2dpSrcDeath_.proxy_ = nullptr;
    }

private:
    A2dpSource::impl &a2dpSrcDeath_;
};

A2dpSource::impl::impl()
{
    HILOGE("A2dpSource::impl::impl start");
    GetProxy();
    if (proxy_ == nullptr) {
        HILOGI("A2dpSource::get proxy_ failed");
        return;
    }
    deathRecipient_ = new BluetoothA2dpSourceDeathRecipient(*this);
    proxy_->AsObject()->AddDeathRecipient(deathRecipient_);

    observerImp_ = new (std::nothrow) BluetoothA2dpSourceObserverImp(*this);
    if (observerImp_ == nullptr) {
        HILOGI("A2dpSource::get proxy_observerImp_ failed");
        return;
    }
    proxy_->RegisterObserver(observerImp_);
};

A2dpSource::impl::~impl()
{
    HILOGD("A2dpSource::impl::~impl  start");
    if (proxy_ != nullptr) {
        proxy_->DeregisterObserver(observerImp_);
        proxy_->AsObject()->RemoveDeathRecipient(deathRecipient_);
    }
}

void A2dpSource::impl::GetProxy()
{
    HILOGI("A2dpSource::impl::GetProxy start");
    sptr<ISystemAbilityManager> samgr = SystemAbilityManagerClient::GetInstance().GetSystemAbilityManager();
    if (!samgr) {
        HILOGE("A2dpSource::impl::GetProxy error: no samgr");
        return;
    }

    sptr<IRemoteObject> hostRemote = samgr->GetSystemAbility(BLUETOOTH_HOST_SYS_ABILITY_ID);
    if (!hostRemote) {
        HILOGE("A2dpSource::impl:GetProxy failed: no hostRemote");
        return;
    }

    sptr<IBluetoothHost> hostProxy = iface_cast<IBluetoothHost>(hostRemote);
    if (!hostProxy) {
        HILOGE("A2dpSource::impl::GetProxy error: host no proxy");
        return;
    }

    sptr<IRemoteObject> remote = hostProxy->GetProfile(PROFILE_A2DP_SRC);
    if (!remote) {
        HILOGE("A2dpSource::impl::GetProxy error: no remote");
        return;
    }

    proxy_ = iface_cast<IBluetoothA2dpSrc>(remote);
    if (!proxy_) {
        HILOGE("A2dpSource::impl::GetProxy error: no proxy");
        return;
    }
}

A2dpSource::A2dpSource()
{
    pimpl = std::make_unique<impl>();
    if (!pimpl) {
        HILOGE("A2dpSource::A2dpSource fails: no pimpl");
    }
}

A2dpSource::~A2dpSource()
{
    HILOGE("A2dpSource::~A2dpSource start");
}

void A2dpSource::RegisterObserver(A2dpSourceObserver *observer)
{
    HILOGD("%{public}s start.", __func__);
    std::shared_ptr<A2dpSourceObserver> pointer(observer, [](A2dpSourceObserver *) {});
    pimpl->observers_.Register(pointer);
}

void A2dpSource::DeregisterObserver(A2dpSourceObserver *observer)
{
    HILOGD("%{public}s start.", __func__);
    std::shared_ptr<A2dpSourceObserver> pointer(observer, [](A2dpSourceObserver *) {});
    pimpl->observers_.Deregister(pointer);
}

std::vector<BluetoothRemoteDevice> A2dpSource::GetDevicesByStates(std::vector<int> states) const
{
    HILOGI("[A2dpSource] %{public}s\n", __func__);
    std::vector<BluetoothRemoteDevice> devices;
    std::vector<BluetoothRawAddress> devicesRaw;

    for (int state : states) {
        if ((static_cast<int>(BTConnectState::CONNECTED) != state) &&
            (static_cast<int>(BTConnectState::CONNECTING) != state) &&
            (static_cast<int>(BTConnectState::DISCONNECTING) != state) &&
            (static_cast<int>(BTConnectState::DISCONNECTED) != state)) {
            HILOGI("[A2dpSource] input parameter error.");
            return devices;
        }
    }

    if (pimpl->proxy_ != nullptr && IS_BT_ENABLED()) {
        pimpl->proxy_->GetDevicesByStates(states);
    }

    for (RawAddress it : devicesRaw) {
        BluetoothRemoteDevice remoteDevice(it.GetAddress(), 0);
        devices.push_back(remoteDevice);
    }

    return devices;
}

int A2dpSource::GetDeviceState(const BluetoothRemoteDevice &device) const
{
    HILOGI("[A2dpSource] %{public}s\n", __func__);

    if (!device.IsValidBluetoothRemoteDevice()) {
        HILOGI("[A2dpSource] input parameter error.");
        return RET_BAD_STATUS;
    }

    int ret = RET_NO_ERROR;
    if (pimpl->proxy_ != nullptr && IS_BT_ENABLED()) {
        ret = pimpl->proxy_->GetDeviceState(RawAddress(device.GetDeviceAddr()));
    } else {
        HILOGI("[A2dpSource] proxy or bt disable.");
        return RET_BAD_STATUS;
    }

    return ret;
}

int A2dpSource::GetPlayingState(const BluetoothRemoteDevice &device) const
{
    HILOGI("[A2dpSource] %{public}s\n", __func__);

    if (!device.IsValidBluetoothRemoteDevice()) {
        HILOGI("[A2dpSource] input parameter error.");
        return RET_BAD_STATUS;
    }

    int ret = RET_NO_ERROR;
    if (pimpl->proxy_ != nullptr && IS_BT_ENABLED()) {
        ret = pimpl->proxy_->GetPlayingState(RawAddress(device.GetDeviceAddr()));
    } else {
        HILOGI("[A2dpSource] proxy or bt disable.");
        return RET_BAD_STATUS;
    }

    return ret;
}

bool A2dpSource::Connect(const BluetoothRemoteDevice &device)
{
    HILOGI("[A2dpSource] %{public}s\n", __func__);

    if (!device.IsValidBluetoothRemoteDevice()) {
        HILOGI("[A2dpSource] input parameter error.");
        return false;
    }

    int ret = RET_NO_ERROR;
    if (pimpl->proxy_ != nullptr && IS_BT_ENABLED()) {
        ret= pimpl->proxy_->Connect(RawAddress(device.GetDeviceAddr()));
    } else {
        HILOGI("[A2dpSource] proxy or bt disable.");
        return false;
    }
    return (RET_NO_ERROR == ret);
}

bool A2dpSource::Disconnect(const BluetoothRemoteDevice &device)
{
    HILOGI("[A2dpSource] %{public}s\n", __func__);

    if (!device.IsValidBluetoothRemoteDevice()) {
        HILOGI("[A2dpSource] input parameter error.");
        return false;
    }

    int ret = RET_NO_ERROR;
    if (pimpl->proxy_ != nullptr && IS_BT_ENABLED()) {
        ret = pimpl->proxy_->Disconnect(RawAddress(device.GetDeviceAddr()));
    } else {
        HILOGI("[A2dpSource] proxy or bt disable.");
        return false;
    }
    return (RET_NO_ERROR == ret);
}

A2dpSource *A2dpSource::GetProfile()
{
    HILOGI("[A2dpSource] %{public}s\n", __func__);
    static A2dpSource service;
    return &service;
}

int A2dpSource::SetActiveSinkDevice(const BluetoothRemoteDevice &device)
{
    HILOGI("[A2dpSource] %{public}s\n", __func__);

    if (!device.IsValidBluetoothRemoteDevice()) {
        HILOGI("[A2dpSource] input parameter error.");
        return RET_BAD_PARAM;
    }

    int ret = RET_NO_ERROR;
    if (pimpl->proxy_ != nullptr && IS_BT_ENABLED()) {
        ret = pimpl->proxy_->SetActiveSinkDevice(RawAddress(device.GetDeviceAddr()));
    } else {
        HILOGI("[A2dpSource] proxy or bt disable.");
        return false;
    }

    return ret;
}

const BluetoothRemoteDevice &A2dpSource::GetActiveSinkDevice() const
{
    HILOGI("[A2dpSource] %{public}s", __func__);

    BluetoothRawAddress rawAddress;
    static BluetoothRemoteDevice deviceInfo;

    if (pimpl->proxy_ != nullptr && IS_BT_ENABLED()) {
        pimpl->proxy_->GetActiveSinkDevice();
    }

    deviceInfo = BluetoothRemoteDevice(rawAddress.GetAddress(), 0);
    return deviceInfo;
}

bool A2dpSource::SetConnectStrategy(const BluetoothRemoteDevice &device, int strategy)
{
    HILOGI("[A2dpSource] %{public}s", __func__);

    if ((!device.IsValidBluetoothRemoteDevice()) || (((int)BTStrategyType::CONNECTION_ALLOWED != strategy) &&
                                                        ((int)BTStrategyType::CONNECTION_FORBIDDEN != strategy))) {
        HILOGI("[A2dpSource] input parameter error.");
        return false;
    }

    int ret = RET_NO_ERROR;
    if (pimpl->proxy_ != nullptr && IS_BT_ENABLED()) {
        ret = pimpl->proxy_->SetConnectStrategy(RawAddress(device.GetDeviceAddr()), strategy);
    } else {
        HILOGI("[A2dpSource] proxy or bt disable.");
        return false;
    }

    return (RET_NO_ERROR == ret);
}

int A2dpSource::GetConnectStrategy(const BluetoothRemoteDevice &device) const
{
    HILOGI("[A2dpSource] %{public}s\n", __func__);

    if (!device.IsValidBluetoothRemoteDevice()) {
        HILOGI("[A2dpSource] input parameter error.");
        return RET_BAD_PARAM;
    }

    int ret = RET_NO_ERROR;
    if (pimpl->proxy_ != nullptr && IS_BT_ENABLED()) {
        ret = pimpl->proxy_->GetConnectStrategy(RawAddress(device.GetDeviceAddr()));
    } else {
        HILOGI("[A2dpSource] proxy or bt disable.");
        return RET_BAD_STATUS;
    }

    return ret;
}

A2dpCodecStatus A2dpSource::GetCodecStatus(const BluetoothRemoteDevice &device) const
{
    HILOGI("[A2dpSource] %{public}s\n", __func__);

    A2dpCodecStatus ret;
    BluetoothA2dpCodecStatus codecStatus;
    A2dpCodecInfo serviceInfo;

    if (!device.IsValidBluetoothRemoteDevice()) {
        HILOGI("[A2dpSource] input parameter error.");
        return ret;
    }

    if (pimpl->proxy_ != nullptr && IS_BT_ENABLED()) {
        codecStatus = pimpl->proxy_->GetCodecStatus(RawAddress(device.GetDeviceAddr()));
    }

    ret.codecInfo.codecType = codecStatus.codecInfo.codecType;
    ret.codecInfo.sampleRate = codecStatus.codecInfo.sampleRate;
    ret.codecInfo.channelMode = codecStatus.codecInfo.channelMode;
    ret.codecInfo.codecPriority = codecStatus.codecInfo.codecPriority;
    ret.codecInfo.bitsPerSample = codecStatus.codecInfo.bitsPerSample;

    for (auto it = codecStatus.codecInfoConfirmCap.begin(); it != codecStatus.codecInfoConfirmCap.end(); it++) {
        serviceInfo.codecType = it->codecType;
        serviceInfo.sampleRate = it->sampleRate;
        serviceInfo.channelMode = it->channelMode;
        serviceInfo.codecPriority = it->codecPriority;
        serviceInfo.bitsPerSample = it->bitsPerSample;
        ret.codecInfoConfirmedCap.push_back(serviceInfo);
    }

    for (auto it = codecStatus.codecInfoLocalCap.begin(); it != codecStatus.codecInfoLocalCap.end(); it++) {
        serviceInfo.codecType = it->codecType;
        serviceInfo.sampleRate = it->sampleRate;
        serviceInfo.channelMode = it->channelMode;
        serviceInfo.codecPriority = it->codecPriority;
        serviceInfo.bitsPerSample = it->bitsPerSample;
        ret.codecInfoLocalCap.push_back(serviceInfo);
    }
    return ret;
}

int A2dpSource::SetCodecPreference(const BluetoothRemoteDevice &device, const A2dpCodecInfo &info)
{
    HILOGI("[A2dpSource] %{public}s\n", __func__);

    int ret = RET_NO_ERROR;
    if (!device.IsValidBluetoothRemoteDevice()) {
        HILOGI("[A2dpSource] input parameter error.");
        ret = RET_BAD_PARAM;
        return ret;
    }
    BluetoothA2dpCodecInfo serviceInfo;

    serviceInfo.codecType = info.codecType;
    serviceInfo.sampleRate = info.sampleRate;
    serviceInfo.channelMode = info.channelMode;
    serviceInfo.bitsPerSample = info.bitsPerSample;
    serviceInfo.codecPriority = info.codecPriority;
    serviceInfo.codecSpecific1 = info.codecSpecific1;
    serviceInfo.codecSpecific2 = info.codecSpecific2;
    serviceInfo.codecSpecific3 = info.codecSpecific3;
    serviceInfo.codecSpecific4 = info.codecSpecific4;

    if (pimpl->proxy_ != nullptr && IS_BT_ENABLED()) {
        ret = pimpl->proxy_->SetCodecPreference(RawAddress(device.GetDeviceAddr()), serviceInfo);
    } else {
        HILOGI("[A2dpSource] proxy or bt disable.");
        return RET_BAD_STATUS;
    }

    return ret;
}

void A2dpSource::SwitchOptionalCodecs(const BluetoothRemoteDevice &device, bool isEnable)
{
    HILOGI("[A2dpSource] %{public}s\n", __func__);

    if (!device.IsValidBluetoothRemoteDevice()) {
        HILOGI("[A2dpSource] input parameter error.");
        return;
    }

    if (pimpl->proxy_ != nullptr && IS_BT_ENABLED()) {
        pimpl->proxy_->SwitchOptionalCodecs(RawAddress(device.GetDeviceAddr()), isEnable);
    } else {
        HILOGI("[A2dpSource] proxy or bt disable.");
        return;
    }
}

int A2dpSource::GetOptionalCodecsSupportState(const BluetoothRemoteDevice &device) const
{
    HILOGI("[A2dpSource] %{public}s\n", __func__);

    if (!device.IsValidBluetoothRemoteDevice()) {
        HILOGI("[A2dpSource] input parameter error.");
        return RET_BAD_PARAM;
    }

    int ret = RET_NO_ERROR;
    if (pimpl->proxy_ != nullptr && IS_BT_ENABLED()) {
        pimpl->proxy_->GetOptionalCodecsSupportState(RawAddress(device.GetDeviceAddr()));
    } else {
        HILOGI("[A2dpSource] proxy or bt disable.");
        return RET_BAD_STATUS;
    }

    return ret;
}

int A2dpSource::StartPlaying(const BluetoothRemoteDevice &device)
{
    HILOGI("[A2dpSource] %{public}s\n", __func__);

    if (!device.IsValidBluetoothRemoteDevice()) {
        HILOGI("[A2dpSource] input parameter error.");
        return RET_BAD_PARAM;
    }

    int ret = RET_NO_ERROR;
    if (pimpl->proxy_ != nullptr && IS_BT_ENABLED()) {
        ret = pimpl->proxy_->StartPlaying(RawAddress(device.GetDeviceAddr()));
    } else {
        HILOGI("[A2dpSource] proxy or bt disable.");
        return RET_BAD_STATUS;
    }

    return ret;
}

int A2dpSource::SuspendPlaying(const BluetoothRemoteDevice &device)
{
    HILOGI("[A2dpSource] %{public}s\n", __func__);

    if (!device.IsValidBluetoothRemoteDevice()) {
        HILOGI("[A2dpSource] input parameter error.");
        return RET_BAD_PARAM;
    }

    int ret = RET_NO_ERROR;
    if (pimpl->proxy_ != nullptr && IS_BT_ENABLED()) {
        ret = pimpl->proxy_->SuspendPlaying(RawAddress(device.GetDeviceAddr()));
    } else {
        HILOGI("[A2dpSource] proxy or bt disable.");
        return RET_BAD_STATUS;
    }

    return ret;
}

int A2dpSource::StopPlaying(const BluetoothRemoteDevice &device)
{
    HILOGI("[A2dpSource] %{public}s\n", __func__);

    if (!device.IsValidBluetoothRemoteDevice()) {
        HILOGI("[A2dpSource] input parameter error.");
        return RET_BAD_PARAM;
    }

    int ret = RET_NO_ERROR;
    if (pimpl->proxy_ != nullptr && IS_BT_ENABLED()) {
        ret = pimpl->proxy_->StopPlaying(RawAddress(device.GetDeviceAddr()));
    } else {
        HILOGI("[A2dpSource] proxy or bt disable.");
        return RET_BAD_STATUS;
    }

    return ret;
}

void A2dpSource::SetAudioConfigure(
    const BluetoothRemoteDevice &device, uint32_t sampleRate, uint32_t bits, uint8_t channel)
{
    HILOGI("[A2dpSource] %{public}s\n", __func__);
    if (!device.IsValidBluetoothRemoteDevice()) {
        HILOGI("[A2dpSource] input parameter error.");
        return;
    }

    if (pimpl->proxy_ != nullptr && IS_BT_ENABLED()) {
        pimpl->proxy_->SetAudioConfigure(
            RawAddress(device.GetDeviceAddr()), (int)sampleRate, (int)bits, (int)channel);
    } else {
        HILOGI("[A2dpSource] proxy or bt disable.");
        return;
    }
    return;
}

int A2dpSource::WriteFrame(const uint8_t *data, uint32_t size)
{
    HILOGI("[A2dpSource] %{public}s\n", __func__);
    if (pimpl->proxy_ != nullptr && IS_BT_ENABLED()) {
        return pimpl->proxy_->WriteFrame(data, size);
    } else {
        HILOGI("[A2dpSource] proxy or bt disable.");
        return RET_BAD_STATUS;
    }
    return RET_BAD_STATUS;
}
    
void A2dpSource::GetRenderPosition(uint16_t &delayValue, uint16_t &sendDataSize, uint32_t &timeStamp)
{
    HILOGI("[A2dpSource] %{public}s\n", __func__);
    if (pimpl->proxy_ != nullptr && IS_BT_ENABLED()) {
        pimpl->proxy_->GetRenderPosition(delayValue, sendDataSize, timeStamp);
    } else {
        HILOGI("[A2dpSource] proxy or bt disable.");
        return;
    }
    return;
}

}  // namespace Bluetooth
}  // namespace OHOS