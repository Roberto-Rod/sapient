/*
 * Mercury.cpp
 *
 *  Created on: 11 Jan 2020
 *      Author: rh
 */

#include "mercury.hpp"
#include "sapient.hpp"
#include "sapientmode.hpp"
#include "debuglog.hpp"
#include "board.hpp"

#include "base/baselib/inc/crc16.hpp"
#include "comms/commslib/inc/message.hpp"
#include "control/controllib/inc/commandfunctions.hpp"
#include "control/controllib/inc/datahandler.hpp"
#include "sio/siolib/inc/memoryidevice.hpp"
#include "linuxserialiodevice.hpp"
#include "system/systemlib/inc/mcmcommands.hpp"
#include "system/systemlib/inc/moduleids.hpp"

#include <syslog.h>
#include <cstdio>
#include <cstdarg>

namespace sapient
{
    Mercury::Mercury()
    {
    }

    Mercury::~Mercury()
    {
    }

    void Mercury::operator()(std::string port)
    {
        bool ok(true);
        sio::SerialIoDevice *sio = sio::getSerialIoDevice(port);

        while (ok)
        {
            if (sio->isGood())
            {
                comms::CommsDevice comms(*sio, kMessageTimeout_ms);
                m_pCommsDevice = &comms;

                // Run loop which controls Mercury
                while(sio->isGood())
                {
                    if (ping())
                    {
                        // Check the Sapient mode
                        uint32_t mode(sapient::SapientMode::instance().mode());

                        // Does the Sapient side want us to be jamming?
                        if (mode > 0)
                        {
                            // Is the right mission file already loaded?
                            bool reloadMission(true);
                            system::McmState::State state(getTargetState());
                            if (!system::McmState::isZeroized(state))
                            {
                                std::string sapientMission, mercuryMission;
                                sapient::SapientMode::instance().getMissionName(mode, sapientMission);
                                if (getMissionName(mercuryMission))
                                {
                                    reloadMission = (sapientMission != mercuryMission);
                                }
                                else
                                {
                                    log(LOG_WARNING, "failed to retrieve mission name from jammer");
                                }
                            }
                            if (reloadMission)
                            {
                                stopJamming();
                                waitReadyForMission();
                                std::string file;
                                sapient::SapientMode::instance().getMissionFileName(mode, file);
                                log(LOG_INFO, "sending %s", file.c_str());
                                sendMission(file);
                            }

                            // Start jamming
                            if (!system::McmState::isJammingOrRequested(state))
                            {
                                startJamming();
                            }
                        }
                        else
                        {
                            system::McmState::State state(getTargetState());
                            if (system::McmState::isJamming(state))
                            {
                                stopJamming();
                            }
                        }
                    }
                    else
                    {
                        log(LOG_WARNING, "jammer ping failed");
                    }
                }
                m_state = state::SerialDisconnected;
                m_pCommsDevice = nullptr;
            }

            // Wait before attempting to reconnect to serial port
            ::sleep(2);
            sio->reinitialise();
        }
    }

    bool Mercury::waitReadyForMission()
    {
        bool ready(false);
        uint32_t start(board::systemTimeMs());

        // Wait for system to be available
        bool keepWaiting(true);
        while (!ready && keepWaiting)
        {
            keepWaiting = ((board::systemTimeMs() - start) <= kWaitReadyTime_ms);
            if (keepWaiting && m_pCommsDevice->getIoDevice().isGood())
            {
                // Ping
                if (ping())
                {
                    log(LOG_INFO, "ping OK");

                    // Check Target Version
                    if (checkTargetVersion())
                    {
                        log(LOG_INFO, "target version OK");

                        // Check State
                        system::McmState::State state(getTargetState());
                        log(LOG_INFO, "target state %u", state);

                        // Drop the "operational" state from ready states as this is seen briefly before installation of
                        // pre-loaded mission. Starting mission upload before installation of pre-loaded
                        // mission leads to data upload failure when installation of pre-loaded mission starts.
                        if (system::McmState::isReadyForNewMission(state) && !system::McmState::isOperational(state))
                        {
                            m_state = Mercury::state::ReadyForMission;
                            ready = true;
                            log(LOG_INFO, "target system ready for new mission");
                        }
                        else if (state == system::McmState::Unknown)
                        {
                            m_state = Mercury::state::NoResponse;
                            log(LOG_WARNING, "get target state failed");
                        }
                        else if (system::McmState::isStartup(state))
                        {
                            m_state = Mercury::state::NotReadyForMission;
                            log(LOG_WARNING, "target system starting-up");
                        }
                        else
                        {
                            m_state = Mercury::state::NotReadyForMission;
                            log(LOG_WARNING, "target system not ready for new mission");
                        }
                    }
                    else
                    {
                        m_state = Mercury::state::NoResponse;
                        log(LOG_WARNING, "target version fail");
                    }
                }
                else
                {
                    m_state = Mercury::state::NoResponse;
                    log(LOG_WARNING, "ping fail");
                    usleep(kTimeBetweenPings_ms * 1000);
                }
            }
        }

        if (!ready && !keepWaiting)
        {
            log(LOG_WARNING, "timed out waiting for system ready for new mission");
        }

        return ready;
    }

    bool Mercury::sendMission(std::string const& filename)
    {
        bool ok(false);
        int32_t size(0);
        uint16_t crc(0);
        FILE *file(::fopen(filename.c_str(), "rb"));

        getFileSizeAndCrc(file, size, crc);

        if (size > 0)
        {
            if (waitReadyForMission())
            {
                log(LOG_INFO, "upload %u byte mission, crc 0x%04x", size, crc);
                system::UploadMissionCommand cmd(static_cast<uint32_t>(size));
                ok = sendCommandCheckOk(cmd);
                if (!ok)
                {
                    log(LOG_WARNING, "upload mission command failed");
                }
            }

            uint16_t seq(0);
            int32_t totalSent(0);
            while (ok)
            {
                uint8_t buffer[kReadChunkSize];
                int32_t numBytes(0);

                numBytes = ::fread(buffer, 1, sizeof(buffer), file);
                if (numBytes > 0)
                {
                    uint8_t *pBuffer(buffer);
                    while (ok && (numBytes > 0))
                    {
                        comms::Message msg;
                        uint32_t sent (control::DataSender::makeDataMessage(msg, seq++, pBuffer, numBytes, system::Module::MCM));
                        numBytes -= sent;
                        pBuffer += sent;
                        // Add inter-packet delay to mimic PC comms as Mercury can fail upload if we send packets back-to-back
                        //::usleep(kInterPacketDelay_ms * 1000);
                        ok = sendMessageCheckOk(msg);

                        if (ok)
                        {
                            totalSent += sent;
                            log(LOG_INFO, "sent %u bytes (total %d of %d)", sent, totalSent, size);
                        }
                        else
                        {
                            log(LOG_ERR, "data send failed");
                        }
                    }
                }
                else
                {
                    break;
                }
            }

            // Do not do anything between the last data packet and the VerifyMissionFileCrcCommand as
            // a short delay here causes the Mercury system to go into the "Mission Upload Failed" state
            if (ok)
            {
                system::VerifyMissionFileCrcCommand cmd(crc);
                // Add inter-packet delay to mimic FillGun UI comms as Mercury can fail upload if we send packets back-to-back
                ::usleep(kInterPacketDelay_ms * 1000);
                // Increase the reply timeout for the VerifyMissionFileCrcCommand
                m_replyTimeout_ms = kReplyTimeoutCrc_ms;
                ok = sendCommandCheckOk(cmd);
                m_replyTimeout_ms = kReplyTimeoutDefault_ms;

                if (ok)
                {
                    ok = waitMissionInstall();
                }
                else
                {
                    log(LOG_WARNING, "CRC check failed");
                }
            }
            else
            {
                log(LOG_WARNING, "data transfer failed");
            }
        }

        ::fclose(file);

        return ok;
    }

    bool Mercury::getMissionName(std::string &name)
    {
        bool result(false);
        comms::Message msg;

        if (sendCommandGetResponse(system::Command::GetMissionName, msg))
        {
            sio::MemoryInputDevice inDev(msg.payload(), msg.getPayloadLengthBytes());
            sio::serialisation::InputDeviceArchive in(inDev);
            control::Command commandHeader(0u);
            commandHeader.deserialise(in);
            system::GetMissionNameResponse resp;
            resp.deserialise(in);
            if (resp.m_responseId == system::Command::GetMissionName)
            {
                result = (commandHeader.commandId() == system::Command::Ok);
                name.assign(reinterpret_cast<char*>(resp.m_missionName));
            }
        }

        return result;
    }

    bool Mercury::waitMissionInstall()
    {
        bool done(false);
        uint32_t start(board::systemTimeMs());
        uint8_t percentPrev(255);

        while (!done && ((board::systemTimeMs() - start) <= kWaitMissionInstallTime_ms))
        {
            uint8_t percent;
            done = isInstallComplete(percent);
            if (done)
            {
                log(LOG_INFO, "installation completed");
            }
            else if (percent != percentPrev)
            {
                log(LOG_INFO, "installation progress %u%%", percent);
                percentPrev = percent;
            }
        }

        if (done)
        {
            log(LOG_INFO, "mission installed");
        }
        else
        {
            log(LOG_WARNING, "timed out waiting for mission installation");
        }

        return done;
    }



    bool Mercury::sendMessageGetResponse(const comms::Message& msg, comms::Message& resp)
    {
        bool ok(false);

        if (m_pCommsDevice)
        {
            // Ditch any messages that are already in the buffer
            while (m_pCommsDevice->getMessage(resp))
            {
            }

            // Send new message
            if (m_pCommsDevice->sendMessage(msg))
            {
                if (m_pCommsDevice->waitForMessageAvailable(m_replyTimeout_ms, [](){static_cast<sio::LinuxSerialIoDevice*>(sio::getSerialIoDevice(0))->read();}))
                {
                    if (m_pCommsDevice->getMessage(resp))
                    {
                        ok = resp.isCommandMessage() && resp.getRecipient() == system::Module::MCM;
                    }
                }
            }
        }

        return ok;
    }

    bool Mercury::sendMessageCheckOk(const comms::Message& msg)
    {
        comms::Message resp;
        return sendMessageGetResponse(msg, resp) && isOkResponse(resp);
    }

    bool Mercury::sendCommandGetResponse(system::Command::Id cmdId, comms::Message& resp)
    {
        control::Command cmd(cmdId);
        return sendCommandGetResponse(cmd, resp);
    }

    bool Mercury::sendCommandGetResponse(const control::Command& cmd, comms::Message& resp)
    {
        comms::Message msg;
        control::makeCommandMessage(msg, cmd, system::Module::MCM);
        return sendMessageGetResponse(msg, resp);
    }

    bool Mercury::sendCommandCheckOk(system::Command::Id cmdId)
    {
        comms::Message resp;
        return sendCommandGetResponse(cmdId, resp) && isOkResponse(resp);
    }

    bool Mercury::sendCommandCheckOk(const control::Command& cmd)
    {
        comms::Message resp;
        return sendCommandGetResponse(cmd, resp) && isOkResponse(resp);
    }

    bool Mercury::isOkResponse(const comms::Message& msg)
    {
        bool ok(false);

        sio::MemoryInputDevice inDev(msg.payload(), msg.getPayloadLengthBytes());
        sio::serialisation::InputDeviceArchive in(inDev);
        control::Command commandHeader(0u);
        commandHeader.deserialise(in);
        ok = (commandHeader.commandId() == system::Command::Ok);

        if (!ok)
        {
            log(LOG_WARNING, "NOK response from system (0x%02x)", commandHeader.commandId());
        }

        return ok;
    }

    bool Mercury::sendCommandCheckResponse(system::Command::Id cmdId)
    {
        bool ok(false);
        comms::Message msg;

        if (sendCommandGetResponse(cmdId, msg))
        {
            sio::MemoryInputDevice inDev(msg.payload(), msg.getPayloadLengthBytes());
            sio::serialisation::InputDeviceArchive in(inDev);
            control::Command commandHeader(0u);
            commandHeader.deserialise(in);
            if (commandHeader.commandId() == system::Command::Ok)
            {
                system::Response resp;
                resp.deserialise(in);
                if (resp.m_responseId == cmdId)
                {
                    ok = true;
                }
            }
        }

        return ok;
    }

    bool Mercury::ping()
    {
        return sendCommandCheckResponse(system::Command::Ping);
    }

    bool Mercury::isInstallComplete(uint8_t &percent)
    {
        bool result(false);
        comms::Message msg;

        if (sendCommandGetResponse(system::Command::GetMissionFileInstallProgress, msg))
        {
            sio::MemoryInputDevice inDev(msg.payload(), msg.getPayloadLengthBytes());
            sio::serialisation::InputDeviceArchive in(inDev);
            control::Command commandHeader(0u);
            commandHeader.deserialise(in);
            system::GetMissionFileInstallProgressResponse resp;
            resp.deserialise(in);
            if (resp.m_responseId == system::Command::GetMissionFileInstallProgress)
            {
                result = (commandHeader.commandId() == system::Command::NotOk);
                percent = resp.m_percent;
            }
        }

        return result;
    }

    system::McmState::State Mercury::getTargetState()
    {
        system::McmState::State state(system::McmState::Unknown);
        comms::Message msg;

        if (sendCommandGetResponse(system::Command::GetState, msg))
        {
            sio::MemoryInputDevice inDev(msg.payload(), msg.getPayloadLengthBytes());
            sio::serialisation::InputDeviceArchive in(inDev);
            control::Command commandHeader(0u);
            commandHeader.deserialise(in);
            if (commandHeader.commandId() == system::Command::Ok)
            {
                system::GetStateResponse resp;
                resp.deserialise(in);
                if (resp.m_responseId == system::Command::GetState)
                {
                    state = resp.m_state;
                }
            }
        }

        return state;
    }

    bool Mercury::getTargetVersion(base::Version& vers)
    {
        bool ok(false);
        comms::Message msg;

        if (sendCommandGetResponse(system::Command::GetSoftwareVersionNumber, msg))
        {
            sio::MemoryInputDevice inDev(msg.payload(), msg.getPayloadLengthBytes());
            sio::serialisation::InputDeviceArchive in(inDev);
            control::Command commandHeader(0u);
            commandHeader.deserialise(in);
            if (commandHeader.commandId() == system::Command::Ok)
            {
                system::GetSoftwareVersionNumberResponse resp;
                resp.deserialise(in);
                if (resp.m_responseId == system::Command::GetSoftwareVersionNumber)
                {
                    vers.m_major = resp.m_majorVersion;
                    vers.m_minor = resp.m_minorVersion;
                    vers.m_build = resp.m_buildNumber;
                    ok = true;
                    log(LOG_INFO, "detected target version %d.%d.%d", vers.m_major, vers.m_minor, vers.m_build);
                }
            }
        }

        return ok;
    }

    bool Mercury::checkTargetVersion()
    {
        bool result(false);
        base::Version targetVersion;
        base::Version minTargetVersion(kMinTargetVersionMajor, kMinTargetVersionMinor);

        if (getTargetVersion(targetVersion))
        {
            result = targetVersion >= minTargetVersion;
        }

        return result;
    }

    bool Mercury::startJamming()
    {
        bool ok(sendCommandCheckResponse(system::Command::StartJamming));
        if (ok)
        {
            log(LOG_INFO, "started jamming");
        }
        else
        {
            log(LOG_WARNING, "start jamming failed");
        }

        return ok;
    }

    bool Mercury::stopJamming()
    {
        bool ok(sendCommandCheckResponse(system::Command::StopJamming));
        if (ok)
        {
            log(LOG_INFO, "stopped jamming");
        }
        else
        {
            log(LOG_WARNING, "stop jamming failed");
        }

        return ok;
    }

    void Mercury::getFileSizeAndCrc(FILE *file, int32_t &size, uint16_t &crc)
    {
        size = 0;
        if (file)
        {
            ::fseek(file, 0L, SEEK_END);
            size = ::ftell(file);
            ::fseek(file, 0L, SEEK_SET);
        }
        else
        {
            log(LOG_ERR, "failed to open file");
        }

        if (size > 0)
        {
            // Get mission CRC before sending as MCM expects the verify CRC command in quick succession
            // after the last data packet
            base::Crc16 crc16;
            for (int32_t i = 0; i < size;)
            {
                uint8_t buffer[1024];
                size_t n(::fread(buffer, 1, sizeof(buffer), file));
                crc16.write(buffer, n);
                i += n;
            }
            crc = crc16.read();
            ::fseek(file, 0L, SEEK_SET);
        }
        else
        {
            log(LOG_WARNING, "empty file");
        }
    }
} /* namespace sapient */

