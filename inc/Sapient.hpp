/*
 * SendMission.hpp
 *
 *  Created on: 11 Jan 2020
 *      Author: rh
 */

#ifndef SRC_SAPIENT_HPP_
#define SRC_SAPIENT_HPP_

#include "sapientmessage.hpp"

#include <sys/socket.h>
#include <string>

namespace sapient
{
    class Sapient
    {
    public:
        Sapient();
        virtual ~Sapient();

        void operator()(std::string ipAddress, uint16_t port, bool debugTerminator);

    private:
        static const uint8_t kMessageTerminator = 0;
        static const uint8_t kMessageTerminatorDebug = '@';
        static const int32_t kDefaultSensorId = 6;
        static const uint32_t kHeartbeatDuration_ms = 10000;
        static const uint32_t kRegAckWait_ms = 30000;
        static const int32_t kMessageBufferSize = 64 * 1024; // 64 KB message buffer

        enum class state
        {
            NotConnected,
            Connected,
            Registered
        };

        bool connect(std::string const &ipAddress, uint16_t port);
        void disconnect();
        void sendMessage(SapientMessage &msg);
        static char *getIpString(sockaddr *sa, char *s, size_t maxlen);

        state m_state{state::NotConnected};
        char m_messageBuffer[kMessageBufferSize] {0};
        uint32_t m_messageBufferPos {0};
        int m_sockfd {-1};
        int32_t m_sensorId {0};
        int32_t m_reportId {0};
    };
} /* namespace sapient */

#endif /* SRC_SAPIENT_HPP_ */
