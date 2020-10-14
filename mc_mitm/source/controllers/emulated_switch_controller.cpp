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
#include "emulated_switch_controller.hpp"
#include <memory>

namespace ams::controller {

    namespace {

        const uint8_t rumble_amp_lut[] = {
            0x00, 0x02, 0x03, 0x04, 0x04, 0x05, 0x06, 0x07, 0x09, 0x0a, 0x0c, 0x0e,
            0x11, 0x14, 0x18, 0x1d, 0x1e, 0x1f, 0x21, 0x22, 0x24, 0x25, 0x27, 0x29,
            0x2a, 0x2c, 0x2e, 0x30, 0x32, 0x35, 0x37, 0x39, 0x3b, 0x3c, 0x3d, 0x3f,
            0x40, 0x41, 0x43, 0x44, 0x46, 0x47, 0x49, 0x4a, 0x4c, 0x4e, 0x4f, 0x51,
            0x53, 0x55, 0x57, 0x58, 0x5a, 0x5c, 0x5e, 0x60, 0x63, 0x65, 0x67, 0x69,
            0x6c, 0x6e, 0x70, 0x73, 0x75, 0x78, 0x7a, 0x7d, 0x80, 0x83, 0x86, 0x88,
            0x8b, 0x8e, 0x92, 0x95, 0x98, 0x9b, 0x9f, 0xa2, 0xa6, 0xa9, 0xad, 0xb1,
            0xb5, 0xb9, 0xbd, 0xc1, 0xc5, 0xca, 0xce, 0xd2, 0xd7, 0xdc, 0xe1, 0xe5,
            0xeb, 0xf0, 0xf5, 0xfa, 0xff
        };

        inline void DecodeRumbleValues(const uint8_t enc[], SwitchRumbleData *dec) {
            uint8_t hi_amp_ind = (enc[1] & 0xfe) >> 1;
            uint8_t lo_amp_ind = ((enc[3] - 0x40) << 1) + ((enc[2] & 0x80) >> 7);

            dec->high_band_freq = 0;
            dec->high_band_amp  = rumble_amp_lut[hi_amp_ind];;
            dec->low_band_freq  = 0;
            dec->low_band_amp   = rumble_amp_lut[lo_amp_ind];
        }

    }

    EmulatedSwitchController::EmulatedSwitchController(const bluetooth::Address *address) 
    : SwitchController(address)
    , m_charging(false)
    , m_battery(BATTERY_MAX) { 
        this->ClearControllerState();

        m_colours.body       = {0x32, 0x32, 0x32};
        m_colours.buttons    = {0xe6, 0xe6, 0xe6};
        m_colours.left_grip  = {0x46, 0x46, 0x46};
        m_colours.right_grip = {0x46, 0x46, 0x46};
    };

    void EmulatedSwitchController::ClearControllerState(void) {
        std::memset(&m_buttons, 0, sizeof(m_buttons));
        m_left_stick = this->PackStickData(STICK_ZERO, STICK_ZERO);
        m_right_stick = this->PackStickData(STICK_ZERO, STICK_ZERO);
        std::memset(&m_motion_data, 0, sizeof(m_motion_data));
    }

    Result EmulatedSwitchController::HandleIncomingReport(const bluetooth::HidReport *report) {
        this->UpdateControllerState(report);

        // Prepare Switch report
        s_input_report.size = sizeof(SwitchInputReport0x30) + 1;
        auto switch_report = reinterpret_cast<SwitchReportData *>(s_input_report.data);
        switch_report->id = 0x30;
        switch_report->input0x30.conn_info      = 0;
        switch_report->input0x30.battery        = m_battery | m_charging;
        switch_report->input0x30.buttons        = m_buttons;
        switch_report->input0x30.left_stick     = m_left_stick;
        switch_report->input0x30.right_stick    = m_right_stick;
        std::memcpy(&switch_report->input0x30.motion, &m_motion_data, sizeof(m_motion_data));

        this->ApplyButtonCombos(&switch_report->input0x30.buttons);

        switch_report->input0x30.timer = os::ConvertToTimeSpan(os::GetSystemTick()).GetMilliSeconds() & 0xff;
        return bluetooth::hid::report::WriteHidReportBuffer(&m_address, &s_input_report);
    }

    Result EmulatedSwitchController::HandleOutgoingReport(const bluetooth::HidReport *report) {
        uint8_t cmdId = report->data[0];
        switch (cmdId) {
            case 0x01:  // Subcmd
                R_TRY(this->HandleSubCmdReport(report));
                break;
            case 0x10:  // Rumble
                R_TRY(this->HandleRumbleReport(report));
                break;
            default:
                break;
        }

        return ams::ResultSuccess();
    }

    Result EmulatedSwitchController::HandleSubCmdReport(const bluetooth::HidReport *report) {
        auto switch_report = reinterpret_cast<const SwitchReportData *>(&report->data);

        switch (switch_report->output0x01.subcmd.id) {
            case SubCmd_RequestDeviceInfo:
                R_TRY(this->SubCmdRequestDeviceInfo(report));
                break;
            case SubCmd_SpiFlashRead:
                R_TRY(this->SubCmdSpiFlashRead(report));
                break;
            case SubCmd_SpiFlashWrite:
                R_TRY(this->SubCmdSpiFlashWrite(report));
                break;
            case SubCmd_SpiSectorErase:
                R_TRY(this->SubCmdSpiSectorErase(report));
                break;
            case SubCmd_SetInputReportMode:
                R_TRY(this->SubCmdSetInputReportMode(report));
                break;
            case SubCmd_TriggersElapsedTime:
                R_TRY(this->SubCmdTriggersElapsedTime(report));
                break;
            case SubCmd_SetShipPowerState:
                R_TRY(this->SubCmdSetShipPowerState(report));
                break;
            case SubCmd_SetMcuConfig:
                R_TRY(this->SubCmdSetMcuConfig(report));
                break;
            case SubCmd_SetMcuState:
                R_TRY(this->SubCmdSetMcuState(report));
                break;
            case SubCmd_SetPlayerLeds:
                R_TRY(this->SubCmdSetPlayerLeds(report));
                break;
            case SubCmd_SetHomeLed:
                R_TRY(this->SubCmdSetHomeLed(report));
                break;
            case SubCmd_EnableImu:
                R_TRY(this->SubCmdEnableImu(report));
                break;
            case SubCmd_EnableVibration:
                R_TRY(this->SubCmdEnableVibration(report));
                break;
            default:
                break;
        }

        return ams::ResultSuccess();
    }

    Result EmulatedSwitchController::HandleRumbleReport(const bluetooth::HidReport *report) {
        auto report_data = reinterpret_cast<const SwitchReportData *>(report->data);
        
        SwitchRumbleData left_motor;
        DecodeRumbleValues(report_data->output0x10.left_motor, &left_motor);

        SwitchRumbleData right_motor;
        DecodeRumbleValues(report_data->output0x10.left_motor, &left_motor);

        return this->SetVibration(&left_motor, &right_motor);
    }

    Result EmulatedSwitchController::SubCmdRequestDeviceInfo(const bluetooth::HidReport *report) {
        const SwitchSubcommandResponse response = {
            .ack = 0x82, 
            .id = SubCmd_RequestDeviceInfo,
            .device_info = {
                .fw_ver = {
                    .major = 0x03,
                    .minor = 0x48
                },
                .type = 0x03, 
                ._unk0 = 0x02, 
                .address = m_address, 
                ._unk1 = 0x01, 
                ._unk2 = 0x02
            }
        };
        
        return this->FakeSubCmdResponse(&response);
    }

    Result EmulatedSwitchController::SubCmdSpiFlashRead(const bluetooth::HidReport *report) {
        // These are read from official Pro Controller
        // @ 0x00006000: ff ff ff ff ff ff ff ff ff ff ff ff ff ff ff ff                            <= Serial 
        // @ 0x00006050: 32 32 32 ff ff ff ff ff ff ff ff ff                                        <= RGB colours (body, buttons, left grip, right grip)
        // @ 0x00006080: 50 fd 00 00 c6 0f 0f 30 61 ae 90 d9 d4 14 54 41 15 54 c7 79 9c 33 36 63    <= Factory Sensor and Stick device parameters
        // @ 0x00006098: 0f 30 61 ae 90 d9 d4 14 54 41 15 54 c7 79 9c 33 36 63                      <= Stick device parameters 2. Normally the same with 1, even in Pro Contr.
        // @ 0x00008010: ff ff ff ff ff ff ff ff ff ff ff ff ff ff ff ff ff ff ff ff ff ff ff ff    <= User Analog sticks calibration
        // @ 0x0000603d: e6 a5 67 1a 58 78 50 56 60 1a f8 7f 20 c6 63 d5 15 5e ff 32 32 32 ff ff ff <= Left analog stick calibration
        // @ 0x00006020: 64 ff 33 00 b8 01 00 40 00 40 00 40 17 00 d7 ff bd ff 3b 34 3b 34 3b 34    <= 6-Axis motion sensor Factory calibration

        auto switch_report = reinterpret_cast<const SwitchReportData *>(&report->data);
        uint32_t read_addr = switch_report->output0x01.subcmd.spi_flash_read.address;
        uint8_t  read_size = switch_report->output0x01.subcmd.spi_flash_read.size;

        SwitchSubcommandResponse response = {
            .ack = 0x90,
            .id = SubCmd_SpiFlashRead,
            .spi_flash_read = {
                .address = read_addr,
                .size = read_size
            }
        };

        if (read_addr == 0x6050) {
            std::memcpy(response.spi_flash_read.data, &m_colours, sizeof(m_colours)); // Set controller colours
        }
        else {
            std::memset(response.spi_flash_read.data, 0xff, read_size); // Console doesn't seem to mind if response is uninitialised data (0xff)
        }

        return this->FakeSubCmdResponse(&response);
    }

    Result EmulatedSwitchController::SubCmdSpiFlashWrite(const bluetooth::HidReport *report) {
        const SwitchSubcommandResponse response = {
            .ack = 0x80,
            .id = SubCmd_SpiFlashWrite,
            .spi_flash_write = {
                .status = 0x01
            }
        };

        return this->FakeSubCmdResponse(&response);
    }

    Result EmulatedSwitchController::SubCmdSpiSectorErase(const bluetooth::HidReport *report) {
        const SwitchSubcommandResponse response = {
            .ack = 0x80,
            .id = SubCmd_SpiSectorErase,
            .spi_flash_write = {
                .status = 0x01
            }
        };

        return this->FakeSubCmdResponse(&response);
    }

    Result EmulatedSwitchController::SubCmdSetInputReportMode(const bluetooth::HidReport *report) {
        const SwitchSubcommandResponse response = {
            .ack = 0x80,
            .id = SubCmd_SetInputReportMode
        };

        return this->FakeSubCmdResponse(&response);
    }

    Result EmulatedSwitchController::SubCmdTriggersElapsedTime(const bluetooth::HidReport *report) {       
        const SwitchSubcommandResponse response = {
            .ack = 0x83,
            .id = SubCmd_TriggersElapsedTime
        };

        return this->FakeSubCmdResponse(&response);
    }

    Result EmulatedSwitchController::SubCmdSetShipPowerState(const bluetooth::HidReport *report) {
        const SwitchSubcommandResponse response = {
            .ack = 0x80,
            .id = SubCmd_SetShipPowerState,
            .set_ship_power_state = {
                .enabled = false
            }
        };

        return this->FakeSubCmdResponse(&response);
    }

    Result EmulatedSwitchController::SubCmdSetMcuConfig(const bluetooth::HidReport *report) {
        const SwitchSubcommandResponse response = {
            .ack = 0xa0,
            .id = SubCmd_SetMcuConfig,
            .data = {0x01, 0x00, 0xff, 0x00, 0x03, 0x00, 0x05, 0x01, 
                     0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
                     0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
                     0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
                     0x00, 0x5c}
        };

        return this->FakeSubCmdResponse(&response);
    }

    Result EmulatedSwitchController::SubCmdSetMcuState(const bluetooth::HidReport *report) {
        const SwitchSubcommandResponse response = {
            .ack = 0x80,
            .id = SubCmd_SetMcuState
        };

        return this->FakeSubCmdResponse(&response);
    }

    Result EmulatedSwitchController::SubCmdSetPlayerLeds(const bluetooth::HidReport *report) {
        const uint8_t *subCmd = &report->data[10];
        uint8_t led_mask = subCmd[1];
        R_TRY(this->SetPlayerLed(led_mask));

        const SwitchSubcommandResponse response = {
            .ack = 0x80,
            .id = SubCmd_SetPlayerLeds
        };

        return this->FakeSubCmdResponse(&response);
    }

    Result EmulatedSwitchController::SubCmdSetHomeLed(const bluetooth::HidReport *report) {
        const SwitchSubcommandResponse response = {
            .ack = 0x80,
            .id = SubCmd_SetHomeLed
        };

        return this->FakeSubCmdResponse(&response);
    }

    Result EmulatedSwitchController::SubCmdEnableImu(const bluetooth::HidReport *report) {
        const SwitchSubcommandResponse response = {
            .ack = 0x80,
            .id = SubCmd_EnableImu
        };

        return this->FakeSubCmdResponse(&response);
    }

    Result EmulatedSwitchController::SubCmdEnableVibration(const bluetooth::HidReport *report) {
        const SwitchSubcommandResponse response = {
            .ack = 0x80,
            .id = SubCmd_EnableVibration
        };

        return this->FakeSubCmdResponse(&response);
    }

    Result EmulatedSwitchController::FakeSubCmdResponse(const SwitchSubcommandResponse *response) {
        s_input_report.size = sizeof(SwitchInputReport0x21) + 1;
        auto report_data = reinterpret_cast<SwitchReportData *>(s_input_report.data);
        report_data->id = 0x21;
        report_data->input0x21.conn_info   = 0;
        report_data->input0x21.battery     = m_battery | m_charging;
        report_data->input0x21.buttons     = m_buttons;
        report_data->input0x21.left_stick  = m_left_stick;
        report_data->input0x21.right_stick = m_right_stick;
        report_data->input0x21.vibrator    = 0;
        std::memcpy(&report_data->input0x21.response, response, sizeof(SwitchSubcommandResponse));
        report_data->input0x21.timer = os::ConvertToTimeSpan(os::GetSystemTick()).GetMilliSeconds() & 0xff;

        //Write a fake response into the report buffer
        return bluetooth::hid::report::WriteHidReportBuffer(&m_address, &s_input_report);
    }

}
