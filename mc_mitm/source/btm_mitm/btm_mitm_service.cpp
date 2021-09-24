/*
 * Copyright (c) 2020-2021 ndeadly
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#include "btm_mitm_service.hpp"
#include "btm_shim.h"
#include "../controllers/controller_management.hpp"
#include <cstring>

namespace ams::mitm::btm {

    namespace {

        void RenameConnectedDevices(BtmConnectedDeviceV1 devices[], size_t count) {
            for (unsigned int i = 0; i < count; ++i) {
                auto device = &devices[i];
                if (!controller::IsOfficialSwitchControllerName(device->name)) {
                    std::strncpy(device->name, controller::pro_controller_name, sizeof(device->name) - 1);
                }
            }
        }

    }

    Result BtmMitmService::GetDeviceConditionDeprecated1(sf::Out<ams::btm::DeviceConditionV100> out) {
        auto device_condition = reinterpret_cast<BtmDeviceConditionV100 *>(out.GetPointer());
        R_TRY(btmGetDeviceConditionDeprecated1Fwd(this->forward_service.get(), device_condition));
        RenameConnectedDevices(device_condition->devices, device_condition->connected_count);
        return ams::ResultSuccess();
    }

    Result BtmMitmService::GetDeviceConditionDeprecated2(sf::Out<ams::btm::DeviceConditionV510> out) {
        auto device_condition = reinterpret_cast<BtmDeviceConditionV510 *>(out.GetPointer());
        R_TRY(btmGetDeviceConditionDeprecated2Fwd(this->forward_service.get(), device_condition));
        RenameConnectedDevices(device_condition->devices, device_condition->connected_count);
        return ams::ResultSuccess();
    }

    Result BtmMitmService::GetDeviceConditionDeprecated3(sf::Out<ams::btm::DeviceConditionV800> out) {
        auto device_condition = reinterpret_cast<BtmDeviceConditionV800 *>(out.GetPointer());
        R_TRY(btmGetDeviceConditionDeprecated3Fwd(this->forward_service.get(), device_condition));
        RenameConnectedDevices(device_condition->devices, device_condition->connected_count);
        return ams::ResultSuccess();
    }

    Result BtmMitmService::GetDeviceConditionDeprecated4(sf::Out<ams::btm::DeviceConditionV900> out) {
        auto device_condition = reinterpret_cast<BtmDeviceConditionV900 *>(out.GetPointer());
        R_TRY(btmGetDeviceConditionDeprecated4Fwd(this->forward_service.get(), device_condition));
        RenameConnectedDevices(device_condition->devices, device_condition->connected_count);
        return ams::ResultSuccess();
    }

    Result BtmMitmService::GetDeviceCondition(u32 id, const sf::OutArray<ams::btm::ConnectedDevice> &out, sf::Out<s32> total_out) {
        auto device_condition = reinterpret_cast<BtmConnectedDeviceV13 *>(out.GetPointer());
        R_TRY(btmGetDeviceConditionFwd(this->forward_service.get(), id, device_condition, out.GetSize(), total_out.GetPointer()));

        for (int i = 0; i < total_out.GetValue(); ++i) {
            auto device = &device_condition[i];
            if (!controller::IsOfficialSwitchControllerName(device->name)) {
                std::strncpy(device->name, controller::pro_controller_name, sizeof(device->name) - 1);
            }
        }

        return ams::ResultSuccess();
    }

    Result BtmMitmService::GetDeviceInfoDeprecated(sf::Out<ams::btm::DeviceInfoList> out) {
        auto device_info = reinterpret_cast<BtmDeviceInfoList *>(out.GetPointer());
        R_TRY(btmGetDeviceInfoDeprecatedFwd(this->forward_service.get(), device_info));

        for (unsigned int i = 0; i < device_info->device_count; ++i) {
            auto device = &device_info->devices[i];
            if (!controller::IsOfficialSwitchControllerName(device->name.name)) {
                std::strncpy(device->name.name, controller::pro_controller_name, sizeof(device->name) - 1);
            }
        }

        return ams::ResultSuccess();
    }

    Result BtmMitmService::GetDeviceInfo(u32 id, const sf::OutArray<ams::btm::DeviceInfo> &out, sf::Out<s32> total_out) {
        auto device_info = reinterpret_cast<BtmDeviceInfoV13 *>(out.GetPointer());
        R_TRY(btmGetDeviceInfoFwd(this->forward_service.get(), id, device_info, out.GetSize(), total_out.GetPointer()));

        for (int i = 0; i < total_out.GetValue(); ++i) {
            auto device = &device_info[i];
            if (!controller::IsOfficialSwitchControllerName(device->name)) {
                std::strncpy(device->name, controller::pro_controller_name, sizeof(device->name) - 1);
            }
        }

        return ams::ResultSuccess();
    }

}
