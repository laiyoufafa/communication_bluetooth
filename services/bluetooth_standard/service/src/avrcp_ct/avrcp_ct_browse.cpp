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

#include "avrcp_ct_internal.h"

#include "avrcp_ct_browse.h"

namespace bluetooth {
/******************************************************************
 * AvrcCtBrowsePacket                                             *
 ******************************************************************/

AvrcCtBrowsePacket::AvrcCtBrowsePacket()
    : pduId_(AVRC_CT_PDU_ID_INVALID),
      parameterLength_(AVRC_CT_BROWSE_PARAMETER_LENGTH),
      mtu_(AVRC_CT_DEFAULT_BROWSE_MTU_SIZE),
      pkt_(nullptr),
      isValid_(false)
{
    LOG_DEBUG("[AVRCP CT] AvrcCtBrowsePacket::%{public}s", __func__);
}

AvrcCtBrowsePacket::~AvrcCtBrowsePacket()
{
    LOG_DEBUG("[AVRCP CT] AvrcCtBrowsePacket::%{public}s", __func__);

    if (pkt_ != nullptr) {
        PacketFree(pkt_);
        pkt_ = nullptr;
    }
}

const Packet *AvrcCtBrowsePacket::AssemblePacket(void)
{
    LOG_DEBUG("[AVRCP TG] AvrcCtBrowsePacket::%{public}s", __func__);

    return pkt_;
}

bool AvrcCtBrowsePacket::DisassemblePacket(Packet *pkt)
{
    LOG_DEBUG("[AVRCP TG] AvrcCtBrowsePacket::%{public}s", __func__);

    return false;
}

/******************************************************************
 * SetBrowsedPlayer                                               *
 ******************************************************************/
AvrcCtSbpPacket::AvrcCtSbpPacket(uint16_t playerId) : AvrcCtBrowsePacket(), status_(AVRC_ES_CODE_INVALID)
{
    LOG_DEBUG("[AVRCP CT] AvrcCtSbpPacket::%{public}s", __func__);

    pduId_ = AVRC_CT_PDU_ID_SET_BROWSED_PLAYER;
    parameterLength_ = AVRC_CT_SBP_FIXED_CMD_PARAMETER_LENGTH;
    playerId_ = playerId;
}

AvrcCtSbpPacket::AvrcCtSbpPacket(Packet *pkt) : AvrcCtBrowsePacket(), status_(AVRC_ES_CODE_INVALID)
{
    LOG_DEBUG("[AVRCP CT] AvrcCtSbpPacket::%{public}s", __func__);

    pduId_ = AVRC_CT_PDU_ID_SET_BROWSED_PLAYER;

    DisassemblePacket(pkt);
}

AvrcCtSbpPacket::~AvrcCtSbpPacket()
{
    LOG_DEBUG("[AVRCP CT] AvrcCtSbpPacket::%{public}s", __func__);
}

const Packet *AvrcCtSbpPacket::AssemblePacket(void)
{
    LOG_DEBUG("[AVRCP CT] AvrcCtSbpPacket::%{public}s", __func__);

    pkt_ = PacketMalloc(0x00, 0x00, AVRC_CT_SBP_FIXED_CMD_FRAME_SIZE);
    auto buffer = static_cast<uint8_t *>(BufferPtr(PacketContinuousPayload(pkt_)));

    uint16_t offset = 0x0000;
    offset += PushOctets1((buffer + offset), pduId_);
    LOG_DEBUG("[AVRCP CT] pduId_[%x]", pduId_);

    offset += PushOctets2((buffer + offset), parameterLength_);
    LOG_DEBUG("[AVRCP CT] parameterLength_[%{public}d]", parameterLength_);

    offset += PushOctets2((buffer + offset), playerId_);
    LOG_DEBUG("[AVRCP CT] playerId_[%x]", playerId_);

    return pkt_;
}

bool AvrcCtSbpPacket::DisassemblePacket(Packet *pkt)
{
    LOG_DEBUG("[AVRCP CT] AvrcCtSbpPacket::%{public}s", __func__);

    isValid_ = false;
    size_t size = PacketPayloadSize(pkt);
    if (size >= AVRC_CT_SBP_MIN_RSP_FRAME_SIZE) {
        auto buffer = static_cast<uint8_t *>(BufferPtr(PacketContinuousPayload(pkt)));

        uint16_t offset = 0x0000;
        uint64_t payload = 0x00;
        offset += PopOctets1((buffer + offset), payload);
        pduId_ = static_cast<uint8_t>(payload);
        LOG_DEBUG("[AVRCP CT] pduId_[%x]", pduId_);

        offset += PopOctets2((buffer + offset), payload);
        parameterLength_ = static_cast<uint16_t>(payload);
        LOG_DEBUG("[AVRCP CT] parameterLength_[%{public}d]", parameterLength_);

        offset += PopOctets1((buffer + offset), payload);
        status_ = static_cast<uint8_t>(payload);
        LOG_DEBUG("[AVRCP CT] status_[%x]", status_);

        offset += PopOctets2((buffer + offset), payload);
        uidCounter_ = static_cast<uint16_t>(payload);
        LOG_DEBUG("[AVRCP CT] uidCounter_[%{public}d]", uidCounter_);

        offset += PopOctets4((buffer + offset), payload);
        numOfItems_ = static_cast<uint32_t>(payload);
        LOG_DEBUG("[AVRCP CT] numOfItems_[%{public}d]", numOfItems_);

        offset += PopOctets2((buffer + offset), payload);
        auto characterSetId = static_cast<uint16_t>(payload);
        LOG_DEBUG("[AVRCP CT] characterSetId[%x]", characterSetId);
        if (characterSetId != AVRC_MEDIA_CHARACTER_SET_UTF8) {
            isValid_ = false;
            LOG_ERROR("[AVRCP CT] The character set id is not UTF-8!");
        }

        offset += PopOctets1((buffer + offset), payload);
        folderDepth_ = static_cast<uint8_t>(payload);
        LOG_DEBUG("[AVRCP CT] folderDepth_[%{public}d]", folderDepth_);

        DisassemblePacketName(buffer, offset);
    } else {
        LOG_DEBUG("[AVRCP CT] The size of the packet is invalid!");
    }

    return isValid_;
}

void AvrcCtSbpPacket::DisassemblePacketName(uint8_t *buffer, uint16_t offset)
{
    LOG_DEBUG("[AVRCP CT] AvrcCtSbpPacket::%{public}s", __func__);

    uint64_t payload = 0x00;

    isValid_ = false;

    char *tempName = nullptr;
    for (int i = 0; i < folderDepth_; i++) {
        offset += PopOctets2((buffer + offset), payload);
        uint16_t nameLength = static_cast<uint16_t>(payload);
        LOG_DEBUG("[AVRCP TG] nameLength[%{public}d]", nameLength);

        tempName = new char[nameLength + 1];
        for (int j = 0; j < nameLength; j++) {
            offset += PopOctets1((buffer + offset), payload);
            tempName[j] = static_cast<char>(payload);
        }
        tempName[nameLength] = '\0';
        folderNames_.push_back(tempName);
        AvrcpCtSafeDeleteArray(tempName, nameLength + 1);
        LOG_DEBUG("[AVRCP CT] folderName[%{public}s]", folderNames_.back().c_str());
    }

    isValid_ = true;
}

/******************************************************************
 * ChangePath                                                     *
 ******************************************************************/

AvrcCtCpPacket::AvrcCtCpPacket(uint16_t uidCounter, uint8_t direction, uint64_t folderUid)
    : AvrcCtBrowsePacket(), direction_(direction), status_(AVRC_ES_CODE_INVALID)
{
    LOG_DEBUG("[AVRCP CT] AvrcTgCpPacket::%{public}s", __func__);

    pduId_ = AVRC_CT_PDU_ID_CHANGE_PATH;
    parameterLength_ = AVRC_CT_CP_FIXED_CMD_PARAMETER_LENGTH;
    uidCounter_ = uidCounter;
    direction_ = direction;
    folderUid_ = folderUid;
}

AvrcCtCpPacket::AvrcCtCpPacket(Packet *pkt)
    : AvrcCtBrowsePacket(), direction_(AVRC_FOLDER_DIRECTION_INVALID), status_(AVRC_ES_CODE_INVALID)
{
    LOG_DEBUG("[AVRCP CT] AvrcTgCpPacket::%{public}s", __func__);

    pduId_ = AVRC_CT_PDU_ID_CHANGE_PATH;

    DisassemblePacket(pkt);
}

AvrcCtCpPacket::~AvrcCtCpPacket()
{
    LOG_DEBUG("[AVRCP CT] AvrcTgCpPacket::%{public}s", __func__);
}

const Packet *AvrcCtCpPacket::AssemblePacket(void)
{
    LOG_DEBUG("[AVRCP CT] AvrcTgCpPacket::%{public}s", __func__);

    pkt_ = PacketMalloc(0x00, 0x00, AVRC_CT_CP_FIXED_CMD_FRAME_SIZE);
    auto buffer = static_cast<uint8_t *>(BufferPtr(PacketContinuousPayload(pkt_)));

    uint16_t offset = 0x0000;
    offset += PushOctets1((buffer + offset), pduId_);
    LOG_DEBUG("[AVRCP CT] pduId_[%x]", pduId_);

    offset += PushOctets2((buffer + offset), parameterLength_);
    LOG_DEBUG("[AVRCP CT] parameterLength_[%{public}d]", parameterLength_);

    offset += PushOctets2((buffer + offset), uidCounter_);
    LOG_DEBUG("[AVRCP CT] uidCounter_[%{public}d]", uidCounter_);

    offset += PushOctets1((buffer + offset), direction_);
    LOG_DEBUG("[AVRCP CT] direction_[%x]", direction_);

    offset += PushOctets8((buffer + offset), folderUid_);
    LOG_DEBUG("[AVRCP CT] folderUid_[%jx]", folderUid_);

    return pkt_;
}

bool AvrcCtCpPacket::DisassemblePacket(Packet *pkt)
{
    LOG_DEBUG("[AVRCP CT] AvrcTgCpPacket::%{public}s", __func__);

    isValid_ = false;
    size_t size = PacketPayloadSize(pkt);
    if (size >= AVRC_CT_CP_FIXED_RSP_FRAME_SIZE) {
        auto buffer = static_cast<uint8_t *>(BufferPtr(PacketContinuousPayload(pkt)));

        uint16_t offset = 0x0000;
        uint64_t payload = 0x00;
        offset += PopOctets1((buffer + offset), payload);
        pduId_ = static_cast<uint8_t>(payload);
        LOG_DEBUG("[AVRCP CT] pduId_[%x]", pduId_);

        offset += PopOctets2((buffer + offset), payload);
        parameterLength_ = static_cast<uint16_t>(payload);
        LOG_DEBUG("[AVRCP CT] parameterLength_[%{public}d]", parameterLength_);

        offset += PopOctets1((buffer + offset), payload);
        status_ = static_cast<uint8_t>(payload);
        LOG_DEBUG("[AVRCP CT] status_[%x]", status_);

        offset += PopOctets4((buffer + offset), payload);
        numOfItems_ = static_cast<uint32_t>(payload);
        LOG_DEBUG("[AVRCP CT] numOfItems_[%{public}d]", numOfItems_);

        isValid_ = true;
    } else {
        LOG_DEBUG("[AVRCP CT] The size of the packet is invalid!");
    }

    return isValid_;
}

/******************************************************************
 * GetFolderItems                                                 *
 ******************************************************************/

AvrcCtGfiPacket::AvrcCtGfiPacket(
    uint8_t scope, uint32_t startItem, uint32_t endItem, const std::vector<uint32_t> &attributes)
    : AvrcCtBrowsePacket(), scope_(scope), status_(AVRC_ES_CODE_INVALID)
{
    LOG_DEBUG("[AVRCP CT] AvrcCtGfiPacket::%{public}s", __func__);

    pduId_ = AVRC_CT_PDU_ID_GET_FOLDER_ITEMS;
    scope_ = scope;
    startItem_ = startItem;
    endItem_ = endItem;
    attributes_ = attributes;
}

AvrcCtGfiPacket::AvrcCtGfiPacket(Packet *pkt) : AvrcCtBrowsePacket(), scope_(AVRC_MEDIA_SCOPE_INVALID), status_(AVRC_ES_CODE_INVALID)
{
    LOG_DEBUG("[AVRCP CT] AvrcCtGfiPacket::%{public}s", __func__);

    pduId_ = AVRC_CT_PDU_ID_GET_FOLDER_ITEMS;

    DisassemblePacket(pkt);
}

AvrcCtGfiPacket::~AvrcCtGfiPacket()
{
    LOG_DEBUG("[AVRCP CT] AvrcCtGfiPacket::%{public}s", __func__);

    attributes_.clear();
    mpItems_.clear();
    meItems_.clear();
}

const Packet *AvrcCtGfiPacket::AssemblePacket(void)
{
    LOG_DEBUG("[AVRCP CT] AvrcCtGfiPacket::%{public}s", __func__);

    attributeCount_ = attributes_.size();

    pkt_ = PacketMalloc(0x00,
        0x00,
        AVRC_CT_GFI_MIN_CMD_FRAME_SIZE + static_cast<size_t>(attributeCount_) * AVRC_CT_GFI_ATTRIBUTE_ID_SIZE);
    auto buffer = static_cast<uint8_t *>(BufferPtr(PacketContinuousPayload(pkt_)));

    uint16_t offset = 0x0000;
    offset += PushOctets1((buffer + offset), pduId_);
    LOG_DEBUG("[AVRCP CT] pduId_[%x]", pduId_);

    parameterLength_ = AVRC_CT_GFI_FIXED_CMD_PARAMETER_LENGTH + (attributeCount_ * AVRC_CT_GFI_ATTRIBUTE_ID_SIZE);
    offset += PushOctets2((buffer + offset), parameterLength_);
    LOG_DEBUG("[AVRCP CT] parameterLength_[%{public}d]", parameterLength_);

    offset += PushOctets1((buffer + offset), scope_);
    LOG_DEBUG("[AVRCP CT] scope_[%x]", scope_);

    offset += PushOctets4((buffer + offset), startItem_);
    LOG_DEBUG("[AVRCP CT] startItem_[%{public}d]", startItem_);

    offset += PushOctets4((buffer + offset), endItem_);
    LOG_DEBUG("[AVRCP CT] endItem_[%{public}d]", endItem_);

    if (attributes_.size() == 0) {
        attributeCount_ = AVRC_ATTRIBUTE_COUNT_NO;
    } else {
        attributeCount_ = attributes_.size();
    }
    offset += PushOctets1((buffer + offset), attributeCount_);
    LOG_DEBUG("[AVRCP CT] attributeCount_[%{public}d]", attributeCount_);

    for (auto attribute : attributes_) {
        offset += PushOctets4((buffer + offset), attribute);
        LOG_DEBUG("[AVRCP CT] attribute[%x]", attribute);
    }

    return pkt_;
}

bool AvrcCtGfiPacket::DisassemblePacket(Packet *pkt)
{
    LOG_DEBUG("[AVRCP CT] AvrcCtGfiPacket::%{public}s", __func__);

    isValid_ = false;
    size_t size = PacketPayloadSize(pkt);
    if (size >= AVRC_CT_GIA_MIN_RSP_FRAME_SIZE) {
        auto buffer = static_cast<uint8_t *>(BufferPtr(PacketContinuousPayload(pkt)));

        uint16_t offset = 0x0000;
        uint64_t payload = 0x00;
        offset += PopOctets1((buffer + offset), payload);
        pduId_ = static_cast<uint8_t>(payload);
        LOG_DEBUG("[AVRCP CT] pduId_[%x]", pduId_);

        offset += PopOctets2((buffer + offset), payload);
        parameterLength_ = static_cast<uint16_t>(payload);
        LOG_DEBUG("[AVRCP CT] parameterLength_[%{public}d]", parameterLength_);

        offset += PopOctets1((buffer + offset), payload);
        status_ = static_cast<uint8_t>(payload);
        LOG_DEBUG("[AVRCP CT] status_[%x]", status_);

        offset += PopOctets2((buffer + offset), payload);
        uidCounter_ = static_cast<uint16_t>(payload);
        LOG_DEBUG("[AVRCP CT] uidCounter_[%{public}d]", uidCounter_);

        offset += PopOctets2((buffer + offset), payload);
        numOfItems_ = static_cast<uint16_t>(payload);
        LOG_DEBUG("[AVRCP CT] numOfItems_[%{public}d]", numOfItems_);

        uint8_t itemType = AVRC_MEDIA_TYPE_INVALID;
        for (int i = 0; i < numOfItems_; i++) {
            PopOctets1((buffer + offset), payload);
            itemType = static_cast<uint8_t>(payload);
            switch (itemType) {
                case AVRC_MEDIA_TYPE_MEDIA_PLAYER_ITEM:
                    scope_ = AVRC_MEDIA_SCOPE_PLAYER_LIST;
                    offset += DisassembleMpParameter((buffer + offset));
                    break;
                case AVRC_MEDIA_TYPE_FOLDER_ITEM:
                    /// FALL THROUGH
                case AVRC_MEDIA_TYPE_MEDIA_ELEMENT_ITEM:
                    scope_ = AVRC_MEDIA_SCOPE_NOW_PLAYING;
                    offset += DisassembleMeParameter((buffer + offset));
                    break;
                default:
                    LOG_DEBUG("[AVRCP CT] The item type[%x] is invalid!", PopOctets1((buffer + offset), payload));
                    break;
            }
        }
    } else {
        LOG_DEBUG("[AVRCP CT] The size of the packet is invalid!");
    }

    return isValid_;
}

uint16_t AvrcCtGfiPacket::DisassembleMpParameter(uint8_t *buffer)
{
    LOG_DEBUG("[AVRCP CT] AvrcCtGfiPacket::%{public}s", __func__);

    isValid_ = true;

    uint16_t offset = 0x00;
    uint64_t payload = 0x00;
    offset += PopOctets1((buffer + offset), payload);
    auto itemType = static_cast<uint8_t>(payload);
    LOG_DEBUG("[AVRCP CT] itemType[%x]", itemType);

    offset += PopOctets2((buffer + offset), payload);
    auto itemLength = static_cast<uint16_t>(payload);
    LOG_DEBUG("[AVRCP CT] itemLength[%{public}d]", itemLength);

    offset += PopOctets2((buffer + offset), payload);
    auto playerId = static_cast<uint16_t>(payload);
    LOG_DEBUG("[AVRCP CT] playerId[%x]", playerId);

    offset += PopOctets1((buffer + offset), payload);
    auto majorType = static_cast<uint8_t>(payload);
    LOG_DEBUG("[AVRCP CT] majorType[%x]", majorType);

    offset += PopOctets4((buffer + offset), payload);
    auto subType = static_cast<uint32_t>(payload);
    LOG_DEBUG("[AVRCP CT] subType[%x]", subType);

    offset += PopOctets1((buffer + offset), payload);
    auto playStatus = static_cast<uint8_t>(payload);
    LOG_DEBUG("[AVRCP CT] playStatus[%x]", playStatus);

    std::vector<uint8_t> features;
    for (int i = 0; i < AVRC_CT_GFI_VALID_FEATURE_OCTETS; i++) {
        offset += PopOctets1((buffer + offset), payload);
        features.push_back(static_cast<uint8_t>(payload));
        LOG_DEBUG("[AVRCP CT] feature[%x]", features.back());
    }

    offset += PopOctets2((buffer + offset), payload);
    auto characterSetId = static_cast<uint16_t>(payload);
    LOG_DEBUG("[AVRCP CT] characterSetId[%x]", characterSetId);
    if (characterSetId != AVRC_MEDIA_CHARACTER_SET_UTF8) {
        isValid_ = false;
        LOG_ERROR("[AVRCP CT] The character set id is not UTF-8!");
    }

    std::string name;
    offset = DisassembleMpParameterName(buffer, offset, name);

    AvrcMpItem mpItem(itemType, playerId, majorType, subType, playStatus, features, name);
    mpItems_.push_back(mpItem);

    return offset;
}

uint16_t AvrcCtGfiPacket::DisassembleMpParameterName(uint8_t *buffer, uint16_t offset, std::string &name)
{
    LOG_DEBUG("[AVRCP CT] AvrcCtGfiPacket::%{public}s", __func__);

    isValid_ = false;

    uint64_t payload = 0x00;

    offset += PopOctets2((buffer + offset), payload);
    auto nameLength = static_cast<uint16_t>(payload);
    LOG_DEBUG("[AVRCP CT] nameLength[%{public}d]", nameLength);

    char *tempName = nullptr;
    tempName = new char[nameLength + 1];
    for (int j = 0; j < nameLength; j++) {
        offset += PopOctets1((buffer + offset), payload);
        tempName[j] = static_cast<char>(payload);
    }
    tempName[nameLength] = '\0';
    name = std::string(tempName);
    AvrcpCtSafeDeleteArray(tempName, nameLength + 1);
    LOG_DEBUG("[AVRCP CT] name[%{public}s]", name.c_str());

    isValid_ = true;

    return offset;
}

uint16_t AvrcCtGfiPacket::DisassembleMeParameter(uint8_t *buffer)
{
    LOG_DEBUG("[AVRCP CT] AvrcCtGfiPacket::%{public}s", __func__);

    isValid_ = true;

    uint16_t offset = 0x00;
    uint64_t payload = 0x00;
    offset += PopOctets1((buffer + offset), payload);
    auto itemType = static_cast<uint8_t>(payload);
    LOG_DEBUG("[AVRCP CT] itemType[%x]", itemType);

    offset += PopOctets2((buffer + offset), payload);
    auto itemLength = static_cast<uint16_t>(payload);
    LOG_DEBUG("[AVRCP CT] itemLength[%{public}d]", itemLength);

    offset += PopOctets8((buffer + offset), payload);
    auto uid = static_cast<uint64_t>(payload);
    LOG_DEBUG("[AVRCP CT] uid[%jx]", uid);

    offset += PopOctets1((buffer + offset), payload);
    auto type = static_cast<uint8_t>(payload);
    LOG_DEBUG("[AVRCP CT] type[%x]", type);

    uint8_t playable = AVRC_MEDIA_FOLDER_PLAYABLE_RESERVED;
    if (itemType == AVRC_MEDIA_TYPE_FOLDER_ITEM) {
        offset += PopOctets1((buffer + offset), payload);
        playable = static_cast<uint8_t>(payload);
    }
    LOG_DEBUG("[AVRCP CT] playable[%x]", playable);

    offset += PopOctets2((buffer + offset), payload);
    auto characterSetId = static_cast<uint16_t>(payload);
    LOG_DEBUG("[AVRCP CT] characterSetId[%x]", characterSetId);
    if (characterSetId != AVRC_MEDIA_CHARACTER_SET_UTF8) {
        isValid_ = false;
        LOG_ERROR("[AVRCP CT] The character set id is not UTF-8!");
    }

    std::string name;
    offset = DisassembleMeParameterName(buffer, offset, name);

    std::vector<uint32_t> attributes;
    std::vector<std::string> values;
    offset = DisassembleMeParameterAttributesAndValues(buffer, offset, itemType, attributes, values);

    AvrcMeItem meItem(itemType, uid, type, playable, name, attributes, values);
    meItems_.push_back(meItem);

    return offset;
}

uint16_t AvrcCtGfiPacket::DisassembleMeParameterName(uint8_t *buffer, uint16_t offset, std::string &name)
{
    LOG_DEBUG("[AVRCP CT] AvrcCtGfiPacket::%{public}s", __func__);

    isValid_ = false;

    uint64_t payload = 0x00;

    offset += PopOctets2((buffer + offset), payload);
    auto nameLength = static_cast<uint16_t>(payload);
    LOG_DEBUG("[AVRCP CT] nameLength[%{public}d]", nameLength);

    char *tempName = nullptr;
    tempName = new char[nameLength + 1];
    for (int j = 0; j < nameLength; j++) {
        payload = 0x00;
        offset += PopOctets1((buffer + offset), payload);
        tempName[j] = static_cast<char>(payload);
    }
    tempName[nameLength] = '\0';
    name = std::string(tempName);
    AvrcpCtSafeDeleteArray(tempName, nameLength + 1);
    LOG_DEBUG("[AVRCP CT] name[%{public}s]", name.c_str());

    isValid_ = true;

    return offset;
}

uint16_t AvrcCtGfiPacket::DisassembleMeParameterAttributesAndValues(uint8_t *buffer, uint16_t offset, uint8_t itemType,
    std::vector<uint32_t> &attributes, std::vector<std::string> &values)
{
    LOG_DEBUG("[AVRCP CT] AvrcCtGfiPacket::%{public}s", __func__);

    isValid_ = false;

    if (itemType == AVRC_MEDIA_TYPE_MEDIA_ELEMENT_ITEM) {
        uint64_t payload = 0x00;
        offset += PopOctets1((buffer + offset), payload);
        auto numOfAttributes = static_cast<uint8_t>(payload);
        LOG_DEBUG("[AVRCP CT] numOfAttributes[%{public}d]", numOfAttributes);

        for (int i = 0; i < numOfAttributes; i++) {
            offset += PopOctets4((buffer + offset), payload);
            attributes.push_back(static_cast<uint32_t>(payload));
            LOG_DEBUG("[AVRCP CT] attribute[%x]", attributes.back());

            offset += PopOctets2((buffer + offset), payload);
            auto characterSetId = static_cast<uint16_t>(payload);
            LOG_DEBUG("[AVRCP CT] characterSetId[%x]", characterSetId);
            if (characterSetId != AVRC_MEDIA_CHARACTER_SET_UTF8) {
                isValid_ = false;
                LOG_ERROR("[AVRCP CT] The character set id is not UTF-8!");
            }

            offset += PopOctets2((buffer + offset), payload);
            auto valueLength = static_cast<uint16_t>(payload);
            LOG_DEBUG("[AVRCP CT] valueLength[%{public}d]", valueLength);

            char *tempName = nullptr;
            tempName = new char[valueLength + 1];
            for (int j = 0; j < valueLength; j++) {
                offset += PopOctets1((buffer + offset), payload);
                tempName[j] = static_cast<char>(payload);
            }
            tempName[valueLength] = '\0';
            values.push_back(tempName);
            AvrcpCtSafeDeleteArray(tempName, valueLength + 1);
            LOG_DEBUG("[AVRCP CT] value[%{public}s]", values.back().c_str());
        }
    }

    isValid_ = true;

    return offset;
}

/******************************************************************
 * GetItemAttributes                                              *
 ******************************************************************/

AvrcCtGiaPacket::AvrcCtGiaPacket(
    uint8_t scope, uint64_t uid, uint16_t uidCounter, const std::vector<uint32_t> &attributes)
    : AvrcCtBrowsePacket(), scope_(scope), status_(AVRC_ES_CODE_INVALID)
{
    LOG_DEBUG("[AVRCP CT] AvrcCtGiaPacket::%{public}s", __func__);

    pduId_ = AVRC_CT_PDU_ID_GET_ITEM_ATTRIBUTES;
    scope_ = scope;
    uid_ = uid;
    uidCounter_ = uidCounter;
    attributes_ = attributes;
}

AvrcCtGiaPacket::AvrcCtGiaPacket(Packet *pkt) : AvrcCtBrowsePacket(), scope_(AVRC_MEDIA_SCOPE_INVALID), status_(AVRC_ES_CODE_INVALID)
{
    LOG_DEBUG("[AVRCP CT] AvrcCtGiaPacket::%{public}s", __func__);

    pduId_ = AVRC_CT_PDU_ID_GET_ITEM_ATTRIBUTES;

    DisassemblePacket(pkt);
}

AvrcCtGiaPacket::~AvrcCtGiaPacket()
{
    LOG_DEBUG("[AVRCP CT] AvrcCtGiaPacket::%{public}s", __func__);

    attributes_.clear();
    values_.clear();
}

const Packet *AvrcCtGiaPacket::AssemblePacket(void)
{
    LOG_DEBUG("[AVRCP CT] AvrcCtGiaPacket::%{public}s", __func__);

    numOfAttributes_ = attributes_.size();

    pkt_ = PacketMalloc(0x00,
        0x00,
        AVRC_CT_GIA_MIN_CMD_FRAME_SIZE + static_cast<size_t>(numOfAttributes_) * AVRC_CT_GIA_ATTRIBUTE_ID_SIZE);
    auto buffer = static_cast<uint8_t *>(BufferPtr(PacketContinuousPayload(pkt_)));

    uint16_t offset = 0x0000;
    offset += PushOctets1((buffer + offset), pduId_);
    LOG_DEBUG("[AVRCP CT] pduId_[%x]", pduId_);

    parameterLength_ = AVRC_CT_GIA_FIXED_CMD_PARAMETER_LENGTH + (numOfAttributes_ * AVRC_CT_GIA_ATTRIBUTE_ID_SIZE);
    offset += PushOctets2((buffer + offset), parameterLength_);
    LOG_DEBUG("[AVRCP CT] parameterLength_[%{public}d]", parameterLength_);

    offset += PushOctets1((buffer + offset), scope_);
    LOG_DEBUG("[AVRCP CT] scope_[%x]", scope_);

    offset += PushOctets8((buffer + offset), uid_);
    LOG_DEBUG("[AVRCP CT] uid_[%jx]", uid_);

    offset += PushOctets2((buffer + offset), uidCounter_);
    LOG_DEBUG("[AVRCP CT] uidCounter_[%{public}d]", uidCounter_);

    offset += PushOctets1((buffer + offset), numOfAttributes_);
    LOG_DEBUG("[AVRCP CT] numOfAttributes_[%{public}d]", numOfAttributes_);

    for (auto attribute : attributes_) {
        offset += PushOctets4((buffer + offset), attribute);
        LOG_DEBUG("[AVRCP CT] attribute[%x]", attribute);
    }

    return pkt_;
}

bool AvrcCtGiaPacket::DisassemblePacket(Packet *pkt)
{
    LOG_DEBUG("[AVRCP CT] AvrcCtGiaPacket::%{public}s", __func__);

    isValid_ = false;
    size_t size = PacketPayloadSize(pkt);
    if (size >= AVRC_CT_GIA_MIN_RSP_FRAME_SIZE) {
        auto buffer = static_cast<uint8_t *>(BufferPtr(PacketContinuousPayload(pkt)));

        uint16_t offset = 0x0000;
        uint64_t payload = 0x00;
        offset += PopOctets1((buffer + offset), payload);
        pduId_ = static_cast<uint8_t>(payload);
        LOG_DEBUG("[AVRCP CT] pduId_[%x]", pduId_);

        offset += PopOctets2((buffer + offset), payload);
        parameterLength_ = static_cast<uint16_t>(payload);
        LOG_DEBUG("[AVRCP CT] parameterLength_[%{public}d]", parameterLength_);

        offset += PopOctets1((buffer + offset), payload);
        status_ = static_cast<uint8_t>(payload);
        LOG_DEBUG("[AVRCP CT] status_[%x]", status_);

        offset += PopOctets1((buffer + offset), payload);
        numOfAttributes_ = static_cast<uint8_t>(payload);
        LOG_DEBUG("[AVRCP CT] numOfAttributes_[%{public}d]", numOfAttributes_);

        uint16_t characterSetId = AVRC_MEDIA_CHARACTER_SET_UTF8;
        for (int i = 0; i < numOfAttributes_; i++) {
            offset += PopOctets4((buffer + offset), payload);
            attributes_.push_back(static_cast<uint32_t>(payload));
            LOG_DEBUG("[AVRCP CT] attribute[%x]", attributes_.back());

            offset += PopOctets2((buffer + offset), payload);
            characterSetId = static_cast<uint16_t>(payload);
            LOG_DEBUG("[AVRCP CT] characterSetId[%x]", characterSetId);
            if (characterSetId != AVRC_MEDIA_CHARACTER_SET_UTF8) {
                isValid_ = false;
                LOG_ERROR("[AVRCP CT] The character set id is not UTF-8!");
            }

            DisassemblePacketName(buffer, offset);
        }
    } else {
        LOG_DEBUG("[AVRCP CT] The packet is invalid!");
    }

    return isValid_;
}

void AvrcCtGiaPacket::DisassemblePacketName(uint8_t *buffer, uint16_t &offset)
{
    LOG_DEBUG("[AVRCP CT] AvrcCtGiaPacket::%{public}s", __func__);

    isValid_ = false;

    uint64_t payload = 0x00;

    offset += PopOctets2((buffer + offset), payload);
    auto valueLength = static_cast<uint16_t>(payload);
    LOG_DEBUG("[AVRCP CT] valueLength[%{public}d]", valueLength);

    char *tempName = nullptr;
    tempName = new char[valueLength + 1];
    for (int j = 0; j < valueLength; j++) {
        payload = 0x00;
        offset += PopOctets1((buffer + offset), payload);
        tempName[j] = static_cast<char>(payload);
    }
    tempName[valueLength] = '\0';
    values_.push_back(tempName);
    AvrcpCtSafeDeleteArray(tempName, valueLength + 1);
    LOG_DEBUG("[AVRCP CT] value[%{public}s]", values_.back().c_str());

    isValid_ = true;
}

/******************************************************************
 * GetTotalNumberOfItems                                          *
 ******************************************************************/

AvrcCtGtnoiPacket::AvrcCtGtnoiPacket(uint8_t scope) : AvrcCtBrowsePacket(), scope_(scope), status_(AVRC_ES_CODE_INVALID)
{
    LOG_DEBUG("[AVRCP CT] AvrcCtGtnoiPacket::%{public}s", __func__);

    pduId_ = AVRC_CT_PDU_ID_GET_TOTAL_NUMBER_OF_ITEMS;
    parameterLength_ = AVRC_CT_GTNOI_FIXED_CMD_PARAMETER_LENGTH;
    scope_ = scope;
}

AvrcCtGtnoiPacket::AvrcCtGtnoiPacket(Packet *pkt)
    : AvrcCtBrowsePacket(), scope_(AVRC_MEDIA_SCOPE_INVALID), status_(AVRC_ES_CODE_INVALID)
{
    LOG_DEBUG("[AVRCP CT] AvrcCtGtnoiPacket::%{public}s", __func__);

    pduId_ = AVRC_CT_PDU_ID_GET_TOTAL_NUMBER_OF_ITEMS;

    DisassemblePacket(pkt);
}

AvrcCtGtnoiPacket::~AvrcCtGtnoiPacket()
{
    LOG_DEBUG("[AVRCP CT] AvrcCtGtnoiPacket::%{public}s", __func__);
}

const Packet *AvrcCtGtnoiPacket::AssemblePacket(void)
{
    LOG_DEBUG("[AVRCP CT] AvrcCtGtnoiPacket::%{public}s", __func__);

    pkt_ = PacketMalloc(0x00, 0x00, AVRC_CT_GTNOI_FIXED_CMD_FRAME_SIZE);
    auto buffer = static_cast<uint8_t *>(BufferPtr(PacketContinuousPayload(pkt_)));

    uint16_t offset = 0x0000;
    offset += PushOctets1((buffer + offset), pduId_);
    LOG_DEBUG("[AVRCP CT] pduId_[%x]", pduId_);

    offset += PushOctets2((buffer + offset), parameterLength_);
    LOG_DEBUG("[AVRCP CT] parameterLength_[%{public}d]", parameterLength_);

    offset += PushOctets1((buffer + offset), scope_);
    LOG_DEBUG("[AVRCP CT] scope_[%x]", scope_);

    return pkt_;
}

bool AvrcCtGtnoiPacket::DisassemblePacket(Packet *pkt)
{
    LOG_DEBUG("[AVRCP CT] AvrcCtGtnoiPacket::%{public}s", __func__);

    isValid_ = false;
    size_t size = PacketPayloadSize(pkt);
    if (size >= AVRC_CT_GTNOI_FIXED_RSP_FRAME_SIZE) {
        auto buffer = static_cast<uint8_t *>(BufferPtr(PacketContinuousPayload(pkt)));

        uint16_t offset = 0x0000;
        uint64_t payload = 0x00;
        offset += PopOctets1((buffer + offset), payload);
        pduId_ = static_cast<uint8_t>(payload);
        LOG_DEBUG("[AVRCP CT] pduId_[%x]", pduId_);

        offset += PopOctets2((buffer + offset), payload);
        parameterLength_ = static_cast<uint16_t>(payload);
        LOG_DEBUG("[AVRCP CT] parameterLength_[%{public}d]", parameterLength_);

        offset += PopOctets1((buffer + offset), payload);
        status_ = static_cast<uint8_t>(payload);
        LOG_DEBUG("[AVRCP CT] status_[%x]", status_);

        offset += PopOctets2((buffer + offset), payload);
        uidCounter_ = static_cast<uint16_t>(payload);
        LOG_DEBUG("[AVRCP CT] uidCounter_[%{public}d]", uidCounter_);

        offset += PopOctets4((buffer + offset), payload);
        numOfItems_ = static_cast<uint32_t>(payload);
        LOG_DEBUG("[AVRCP CT] numOfItems_[%{public}d]", numOfItems_);

        isValid_ = true;
    } else {
        LOG_DEBUG("[AVRCP CT] The size of the packet is invalid!");
    }

    return isValid_;
}
};  // namespace bluetooth