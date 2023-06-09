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
#include "bluetooth_hfp_ag.h"
#include "napi_bluetooth_hfp_ag.h"
#include "napi_bluetooth_profile.h"

namespace OHOS {
namespace Bluetooth {
using namespace std;

NapiHandsFreeAudioGatewayObserver NapiHandsFreeAudioGateway::observer_;
bool NapiHandsFreeAudioGateway::isRegistered_ = false;

void NapiHandsFreeAudioGateway::DefineHandsFreeAudioGatewayJSClass(napi_env env)
{
    napi_value constructor;
    napi_property_descriptor properties[] = {
        DECLARE_NAPI_FUNCTION("getConnectionDevices", GetConnectionDevices),
        DECLARE_NAPI_FUNCTION("getDeviceState", GetDeviceState),
        DECLARE_NAPI_FUNCTION("connect", Connect),
        DECLARE_NAPI_FUNCTION("disconnect", Disconnect),
        DECLARE_NAPI_FUNCTION("getScoState", GetScoState),
        DECLARE_NAPI_FUNCTION("connectSco", ConnectSco),
        DECLARE_NAPI_FUNCTION("disconnectSco", DisconnectSco),
        DECLARE_NAPI_FUNCTION("on", On),       
        DECLARE_NAPI_FUNCTION("off", Off),
        DECLARE_NAPI_FUNCTION("openVoiceRecognition", OpenVoiceRecognition),
        DECLARE_NAPI_FUNCTION("closeVoiceRecognition", CloseVoiceRecognition),
    };

    napi_define_class(env, "HandsFreeAudioGateway", NAPI_AUTO_LENGTH, HandsFreeAudioGatewayConstructor, nullptr, 
        sizeof(properties) / sizeof(properties[0]), properties, &constructor);
    napi_value napiProfile;
    napi_new_instance(env, constructor, 0, nullptr, &napiProfile);
    NapiProfile::SetProfile(ProfileCode::CODE_BT_PROFILE_HANDS_FREE_AUDIO_GATEWAY, napiProfile);
    HILOGI("DefineHandsFreeAudioGatewayJSClass finished");
}

napi_value NapiHandsFreeAudioGateway::HandsFreeAudioGatewayConstructor(napi_env env, napi_callback_info info)
{
    napi_value thisVar = nullptr;
    napi_get_cb_info(env, info, nullptr, nullptr, &thisVar, nullptr);
    return thisVar;
}

napi_value NapiHandsFreeAudioGateway::On(napi_env env, napi_callback_info info)
{
    HILOGI("On called");
    size_t expectedArgsCount = ARGS_SIZE_TWO;
    size_t argc = expectedArgsCount;
    napi_value argv[ARGS_SIZE_TWO] = {0};
    napi_value thisVar = nullptr;

    napi_value ret = nullptr;
    napi_get_undefined(env, &ret);

    napi_get_cb_info(env, info, &argc, argv, &thisVar, nullptr);
    if (argc != expectedArgsCount) {
        HILOGE("Requires 2 argument.");
        return ret;
    }
    string type;
    if (!ParseString(env, type, argv[PARAM0])) {
        HILOGE("string expected.");
        return ret;
    }
    std::shared_ptr<BluetoothCallbackInfo> callbackInfo = std::make_shared<BluetoothCallbackInfo>();
    callbackInfo->env_ = env;

    napi_valuetype valueType = napi_undefined;
    napi_typeof(env, argv[PARAM1], &valueType);
    if (valueType != napi_function) {
        HILOGE("Wrong argument type. Function expected.");
        return ret;
    }
    napi_create_reference(env, argv[PARAM1], 1, &callbackInfo->callback_);
    observer_.callbackInfos_[type] = callbackInfo;
    HILOGI("%{public}s is registered", type.c_str());

    if (!isRegistered_) {
        HandsFreeAudioGateway *profile = HandsFreeAudioGateway::GetProfile();
        profile->RegisterObserver(&observer_);
        isRegistered_ = true;
    }
    return ret;
}

napi_value NapiHandsFreeAudioGateway::Off(napi_env env, napi_callback_info info)
{
    HILOGI("Off called");
    size_t expectedArgsCount = ARGS_SIZE_ONE;
    size_t argc = expectedArgsCount;
    napi_value argv[ARGS_SIZE_ONE] = {0};
    napi_value thisVar = nullptr;

    napi_value ret = nullptr;
    napi_get_undefined(env, &ret);

    napi_get_cb_info(env, info, &argc, argv, &thisVar, nullptr);
    if (argc != expectedArgsCount) {
        HILOGE("Requires 1 argument.");
        return ret;
    } 
    string type;
    if (!ParseString(env, type, argv[PARAM0])) {
        HILOGE("string expected.");
        return ret;
    }
    observer_.callbackInfos_[type] = nullptr;
    HILOGI("%{public}s is unregistered", type.c_str());
    return ret;
}

napi_value NapiHandsFreeAudioGateway::GetConnectionDevices(napi_env env, napi_callback_info info)
{
    HILOGI("GetConnectionDevices called");
    napi_value ret = nullptr;
    napi_create_array(env, &ret);
    HandsFreeAudioGateway *profile = HandsFreeAudioGateway::GetProfile();
    vector<BluetoothRemoteDevice> devices = profile->GetConnectedDevices();
    vector<string> deviceVector;
    for (auto &device: devices) {
        deviceVector.push_back(device.GetDeviceAddr());
    }
    ConvertStringVectorToJS(env, ret, deviceVector);
    return ret;
}

napi_value NapiHandsFreeAudioGateway::GetDeviceState(napi_env env, napi_callback_info info)
{
    HILOGI("GetDeviceState called");
    size_t expectedArgsCount = ARGS_SIZE_ONE;
    size_t argc = expectedArgsCount;
    napi_value argv[ARGS_SIZE_ONE] = {0};
    napi_value thisVar = nullptr;

    napi_value ret = nullptr;
    napi_get_undefined(env, &ret);

    napi_get_cb_info(env, info, &argc, argv, &thisVar, nullptr);
    if (argc != expectedArgsCount) {
        HILOGE("Requires 1 argument.");
        return ret;
    }
    string deviceId;
    if (!ParseString(env, deviceId, argv[PARAM0])) {
        HILOGE("string expected.");
        return ret;
    }

    HandsFreeAudioGateway *profile = HandsFreeAudioGateway::GetProfile();
    BluetoothRemoteDevice device(deviceId, 1);
    int state = profile->GetDeviceState(device);
    napi_value result = nullptr;
    napi_create_int32(env, GetProfileConnectionState(state), &result);
    return result;
}

napi_value NapiHandsFreeAudioGateway::GetScoState(napi_env env, napi_callback_info info)
{
    HILOGI("GetScoState called");
    size_t expectedArgsCount = ARGS_SIZE_ONE;
    size_t argc = expectedArgsCount;
    napi_value argv[ARGS_SIZE_ONE] = {0};
    napi_value thisVar = nullptr;

    napi_value ret = nullptr;
    napi_get_undefined(env, &ret);

    napi_get_cb_info(env, info, &argc, argv, &thisVar, nullptr);
    if (argc != expectedArgsCount) {
        HILOGE("Requires 1 argument.");
        return ret;
    }
    string deviceId;
    if (!ParseString(env, deviceId, argv[PARAM0])) {
        HILOGE("string expected.");
        return ret;
    }

    HandsFreeAudioGateway *profile = HandsFreeAudioGateway::GetProfile();
    BluetoothRemoteDevice device(deviceId, 1);
    int state = profile->GetScoState(device);
    napi_value result = nullptr;
    napi_create_int32(env, GetScoConnectionState(state), &result);
    return result;
}

napi_value NapiHandsFreeAudioGateway::ConnectSco(napi_env env, napi_callback_info info)
{
    HILOGI("ConnectSco called");
    size_t expectedArgsCount = ARGS_SIZE_ONE;
    size_t argc = expectedArgsCount;
    napi_value argv[ARGS_SIZE_ONE] = {0};
    napi_value thisVar = nullptr;

    napi_value ret = nullptr;
    napi_get_undefined(env, &ret);

    napi_get_cb_info(env, info, &argc, argv, &thisVar, nullptr);
    if (argc != expectedArgsCount) {
        HILOGE("Requires 1 argument.");
        return ret;
    }
    string deviceId;
    if (!ParseString(env, deviceId, argv[PARAM0])) {
        HILOGE("string expected.");
        return ret;
    }

    HandsFreeAudioGateway *profile = HandsFreeAudioGateway::GetProfile();
    BluetoothRemoteDevice device(deviceId, 1);
    bool isOK = profile->SetActiveDevice(device);
    if (isOK) {
        isOK = profile->ConnectSco();
    }
    napi_value result = nullptr;
    napi_get_boolean(env, isOK, &result);
    return result;
}

napi_value NapiHandsFreeAudioGateway::DisconnectSco(napi_env env, napi_callback_info info)
{
    HILOGI("DisconnectSco called");
    size_t expectedArgsCount = ARGS_SIZE_ONE;
    size_t argc = expectedArgsCount;
    napi_value argv[ARGS_SIZE_ONE] = {0};
    napi_value thisVar = nullptr;

    napi_value ret = nullptr;
    napi_get_undefined(env, &ret);

    napi_get_cb_info(env, info, &argc, argv, &thisVar, nullptr);
    if (argc != expectedArgsCount) {
        HILOGE("Requires 1 argument.");
        return ret;
    }
    string deviceId;
    if (!ParseString(env, deviceId, argv[PARAM0])) {
        HILOGE("string expected.");
        return ret;
    }

    HandsFreeAudioGateway *profile = HandsFreeAudioGateway::GetProfile();
    BluetoothRemoteDevice device(deviceId, 1);
    bool isOK = profile->SetActiveDevice(device);
    if (isOK) {
        isOK = profile->DisconnectSco();
    }
    napi_value result = nullptr;
    napi_get_boolean(env, isOK, &result);
    return result;
}

napi_value NapiHandsFreeAudioGateway::OpenVoiceRecognition(napi_env env, napi_callback_info info)
{
    HILOGI("OpenVoiceRecognition called");
    size_t expectedArgsCount = ARGS_SIZE_ONE;
    size_t argc = expectedArgsCount;
    napi_value argv[ARGS_SIZE_ONE] = {0};
    napi_value thisVar = nullptr;

    napi_value ret = nullptr;
    napi_get_undefined(env, &ret);

    napi_get_cb_info(env, info, &argc, argv, &thisVar, nullptr);
    if (argc != expectedArgsCount) {
        HILOGE("Requires 1 argument.");
        return ret;
    }
    string deviceId;
    if (!ParseString(env, deviceId, argv[PARAM0])) {
        HILOGE("string expected.");
        return ret;
    }

    HandsFreeAudioGateway *profile = HandsFreeAudioGateway::GetProfile();
    BluetoothRemoteDevice device(deviceId, 1);
    bool isOK = profile->OpenVoiceRecognition(device);
    napi_value result = nullptr;
    napi_get_boolean(env, isOK, &result);
    return result;
}

napi_value NapiHandsFreeAudioGateway::CloseVoiceRecognition(napi_env env, napi_callback_info info)
{
    HILOGI("CloseVoiceRecognition called");
    size_t expectedArgsCount = ARGS_SIZE_ONE;
    size_t argc = expectedArgsCount;
    napi_value argv[ARGS_SIZE_ONE] = {0};
    napi_value thisVar = nullptr;

    napi_value ret = nullptr;
    napi_get_undefined(env, &ret);

    napi_get_cb_info(env, info, &argc, argv, &thisVar, nullptr);
    if (argc != expectedArgsCount) {
        HILOGE("Requires 1 argument.");
        return ret;
    }
    string deviceId;
    if (!ParseString(env, deviceId, argv[PARAM0])) {
        HILOGE("string expected.");
        return ret;
    }

    HandsFreeAudioGateway *profile = HandsFreeAudioGateway::GetProfile();
    BluetoothRemoteDevice device(deviceId, 1);
    bool isOK = profile->CloseVoiceRecognition(device);
    napi_value result = nullptr;
    napi_get_boolean(env, isOK, &result);
    return result;
}

napi_value NapiHandsFreeAudioGateway::Connect(napi_env env, napi_callback_info info)
{
    HILOGI("Connect called");
    size_t expectedArgsCount = ARGS_SIZE_ONE;
    size_t argc = expectedArgsCount;
    napi_value argv[ARGS_SIZE_ONE] = {0};
    napi_value thisVar = nullptr;

    napi_value ret = nullptr;
    napi_get_undefined(env, &ret);

    napi_get_cb_info(env, info, &argc, argv, &thisVar, nullptr);
    if (argc != expectedArgsCount) {
        HILOGE("Requires 1 argument.");
        return ret;
    }
    string deviceId;
    if (!ParseString(env, deviceId, argv[PARAM0])) {
        HILOGE("string expected.");
        return ret;
    }

    HandsFreeAudioGateway *profile = HandsFreeAudioGateway::GetProfile();
    BluetoothRemoteDevice device(deviceId, 1);
    bool res = profile->Connect(device);

    napi_value result = nullptr;
    napi_get_boolean(env, res, &result);
    return result;
}

napi_value NapiHandsFreeAudioGateway::Disconnect(napi_env env, napi_callback_info info)
{
    HILOGI("Disconnect called");
    size_t expectedArgsCount = ARGS_SIZE_ONE;
    size_t argc = expectedArgsCount;
    napi_value argv[ARGS_SIZE_ONE] = {0};
    napi_value thisVar = nullptr;

    napi_value ret = nullptr;
    napi_get_undefined(env, &ret);

    napi_get_cb_info(env, info, &argc, argv, &thisVar, nullptr);
    if (argc != expectedArgsCount) {
        HILOGE("Requires 1 argument.");
        return ret;
    }
    string deviceId;
    if (!ParseString(env, deviceId, argv[PARAM0])) {
        HILOGE("string expected.");
        return ret;
    }

    HandsFreeAudioGateway *profile = HandsFreeAudioGateway::GetProfile();
    BluetoothRemoteDevice device(deviceId, 1);
    bool res = profile->Disconnect(device);

    napi_value result = nullptr;
    napi_get_boolean(env, res, &result);
    return result;
}
} // namespace Bluetooth
} // namespace OHOS