/*
 * SendMission.hpp
 *
 *  Created on: 11 Jan 2020
 *      Author: rh
 */

#ifndef SRC_MERCURY_HPP_
#define SRC_MERCURY_HPP_

#include "base/baselib/inc/version.hpp"
#include "comms/commslib/inc/commsdevice.hpp"
#include "comms/commslib/inc/message.hpp"
#include "control/controllib/inc/command.hpp"
#include "system/systemlib/inc/commands.hpp"
#include "system/systemlib/inc/mcmstates.hpp"

#include <cstdint>

using namespace mercury::embedded;

namespace sapient
{
    class Mercury
    {
    public:
        Mercury();
        virtual ~Mercury();

        void operator()(std::string port);
        bool ping();
        bool startJamming();
        bool stopJamming();
        bool sendMission(std::string const &filename);
        bool getMissionName(std::string &name);

    private:
        static const uint32_t kMessageTimeout_ms = 2000; //!< Time, in milliseconds, to flush a partially received message from the buffer
        static const uint32_t kReplyTimeoutDefault_ms = 2500; //!< Time, in milliseconds, to wait for general replies from the target system
        static const uint32_t kReplyTimeoutCrc_ms = 8100; //!< Time, in milliseconds, to wait for VerifyMissionFileCrcCommand replies from the target system
        static const uint32_t kInterPacketDelay_ms = 15;//!< Time, in milliseconds, to delay between data/CRC packets
        static const uint32_t kWaitReadyTime_ms = 300000; //!< Time, in milliseconds, to wait for system to be ready for mission file
        static const uint32_t kWaitMissionInstallTime_ms = 300000; //!< Time, in milliseconds, to wait for mission to be installed across system
        static const uint32_t kTimeBetweenPings_ms = 500; //!< Time, in milliseconds, between pings when waiting for system to come online
        static const int32_t kUartNumber = 0; //!< UART used for comms to target system
        static const int32_t kReadChunkSize = 253; //!< Read mission file in chunks which fit max message payload size
        static const uint16_t kMinTargetVersionMajor = 6; //!< Minimum target version, major part
        static const uint16_t kMinTargetVersionMinor = 5; //!< Minimum target version, minor part

        enum class state
        {
            SerialDisconnected,
            NoResponse,
            NotReadyForMission,
            ReadyForMission,
            Jamming
        };

        state m_state{state::SerialDisconnected};
        comms::CommsDevice *m_pCommsDevice{nullptr};
        uint32_t m_replyTimeout_ms{kReplyTimeoutDefault_ms};

        bool waitReadyForMission();
        bool waitMissionInstall();
        bool sendMessageGetResponse(const comms::Message& msg, comms::Message& resp);
        bool sendMessageCheckOk(const comms::Message& msg);
        bool sendCommandGetResponse(system::Command::Id cmdId, comms::Message& resp);
        bool sendCommandGetResponse(const control::Command& cmd, comms::Message& resp);
        bool sendCommandCheckOk(system::Command::Id cmdId);
        bool sendCommandCheckOk(const control::Command& cmd);
        bool isOkResponse(const comms::Message& msg);
        bool sendCommandCheckResponse(system::Command::Id cmdId);
        bool isInstallComplete(uint8_t &percent);
        system::McmState::State getTargetState();
        bool getTargetVersion(base::Version& vers);
        bool checkTargetVersion();
        static void getFileSizeAndCrc(FILE *file, int32_t &size, uint16_t &crc);
    };
} /* namespace sapient */

#endif /* SRC_MERCURY_HPP_ */
