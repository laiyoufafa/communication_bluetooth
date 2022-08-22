/*
 * Copyright (C) 2021 Huawei Device Co., Ltd.
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

#ifndef NAPI_BLUETOOTH_A2DP_SRC_OBSERVER_H_
#define NAPI_BLUETOOTH_A2DP_SRC_OBSERVER_H_

#include "bluetooth_a2dp_src.h"
#include "napi_bluetooth_utils.h"

namespace OHOS {
namespace Bluetooth {
const std::string STR_BT_A2DP_SOURCE_CONNECTION_STATE_CHANGE = "connectionStateChange";


class NapiA2dpSourceObserver : public A2dpSourceObserver{
public:
    void OnConnectionStateChanged(const BluetoothRemoteDevice &device, int state) override;
    
    NapiA2dpSourceObserver() = default;
    virtual ~NapiA2dpSourceObserver() = default;
    std::map<std::string, std::shared_ptr<BluetoothCallbackInfo>> callbackInfos_ = {};
};

}  // namespace Bluetooth
}  // namespace OHOS
#endif /* NAPI_BLUETOOTH_A2DP_SRC_OBSERVER_H_ */