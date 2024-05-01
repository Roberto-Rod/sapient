#include "sapient.hpp"
#include "sapientmode.hpp"
#include "sapientmessage.hpp"
#include "debuglog.hpp"
#include "base/baselib/inc/circbuffer.hpp"

#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <time.h>
#include <iostream>
#include <memory>
#include <chrono>

// Undef macro to stop call to ::htons failing at higher opimisation levels
#undef htons

namespace sapient
{
    Sapient::Sapient()
    {
    }

    Sapient::~Sapient()
    {
    }

    void Sapient::operator()(std::string ipAddress, uint16_t port, bool debugTerminator)
    {
        int n(0);
        char recvBuff[32 * 1024] = {0};
        bool ok(true);

        while (ok)
        {
            if (connect(ipAddress, port))
            {
                auto lastHeartbeat(std::chrono::steady_clock::now());
                m_reportId = 0;

                // Connected - send registration message with the pre-assigned sensor ID
                SapientMessageSensorRegistration reg;
                reg.m_sensorId = kDefaultSensorId;
                reg.m_sensorIdSet = true;
                sendMessage(reg);

                while(m_state != state::NotConnected)
                {
                    auto now(std::chrono::steady_clock::now());

                    if (m_state == state::Registered)
                    {
                        if (std::chrono::duration_cast<std::chrono::milliseconds>(now - lastHeartbeat).count() >= kHeartbeatDuration_ms)
                        {
                            log(LOG_INFO, "sending heartbeat");
                            SapientMessageHeartbeat hb;
                            hb.m_sensorId = m_sensorId;
                            hb.m_reportId = m_reportId++;
                            sendMessage(hb);
                            lastHeartbeat = now;
                        }
                    }
                    else if (std::chrono::duration_cast<std::chrono::milliseconds>(now - lastHeartbeat).count() >= kRegAckWait_ms)
                    {
                        log(LOG_WARNING, "timed out waiting for registration acknowledgement");
                        m_state = state::NotConnected;
                    }

                    if (m_state != state::NotConnected)
                    {
                        n = ::read(m_sockfd, recvBuff, sizeof(recvBuff));
                        if (n == 0)
                        {
                            m_state = state::NotConnected;
                        }

                        for (int i = 0; i < n; ++i)
                        {
                            // Write received data into message buffer
                            m_messageBuffer[m_messageBufferPos++] = recvBuff[i];

                            // Did we just write a null terminator into the message buffer?
                            // If so then process the message that leads up to that terminator
                            if (m_messageBuffer[m_messageBufferPos-1] == (debugTerminator ? kMessageTerminatorDebug : kMessageTerminator))
                            {
                                // Set the character to a null terminator to cover cases where the terminator
                                // has been substituted for another character
                                m_messageBuffer[m_messageBufferPos-1] = 0;

                                // Reset message buffer
                                m_messageBufferPos = 0;

                                // Do not process empty message
                                // TODO: filter out messages which are completely empty or just have newline characters
                                if (true)
                                {
                                    log(LOG_INFO, "message received");
                                    std::shared_ptr<SapientMessage> msg(sapientMessageFactory(m_messageBuffer));

                                    // Registration Ack received
                                    if (std::dynamic_pointer_cast<SapientMessageSensorRegistrationAck>(msg))
                                    {
                                        m_sensorId = std::dynamic_pointer_cast<SapientMessageSensorRegistrationAck>(msg)->m_sensorId;
                                        m_state = state::Registered;
                                        log(LOG_INFO, "registration acknowledged, sensor ID: %u", m_sensorId);
                                        // TODO: when we know that registration ack is returning good sensor ID then remove 2 lines below
                                        m_sensorId = kDefaultSensorId;
                                        log(LOG_INFO, "using sensor ID: %d", m_sensorId);
                                    }

                                    // Only process tasks when registered with server
                                    if (m_state == state::Registered)
                                    {
                                        // Sensor Task received
                                        if (std::dynamic_pointer_cast<SapientMessageSensorTask>(msg))
                                        {
                                            std::shared_ptr<SapientMessageSensorTask> task = std::dynamic_pointer_cast<SapientMessageSensorTask>(msg);

                                            if (task->m_sensorId == m_sensorId)
                                            {
                                                log(LOG_INFO, "sensor task message received, mode %u", task->m_mode);
                                                SapientMode::instance().setMode(task->m_mode);
                                            }
                                            else
                                            {
                                                log(LOG_WARNING, "received task with wrong sensor ID (task %u, ours %u)", task->m_sensorId, m_sensorId);
                                            }
                                        }
                                    }
                                }
                            }
                            else if (n == 0)
                            {
                                m_state = state::NotConnected;
                            }
                        }
                    }
                };

                disconnect();
            }

            // Wait 10 seconds before re-attempting connection
            for (int i = 10; i > 0; i--)
            {
                log(LOG_WARNING, "SDA not available, retrying in %u second%s...", i, (i > 1 ? "s" : ""));
                ::sleep(1);
            }
        }
    }

    bool Sapient::connect(std::string const &ipAddress, uint16_t port)
    {
        sockaddr_in serv_addr = {0};
        bool ok(false);

        log(LOG_INFO, "connecting to %s:%u", ipAddress.c_str(), port);

        serv_addr.sin_family = AF_INET;
        serv_addr.sin_port = ::htons(port);

        if (::inet_pton(AF_INET, ipAddress.c_str(), &serv_addr.sin_addr) != 1)
        {
            log(LOG_ERR, "inet_pton failed");
        }
        else if ((m_sockfd = ::socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0)) >= 0)
        {
            if (::connect(m_sockfd, reinterpret_cast<sockaddr *>(&serv_addr), sizeof(serv_addr)) >= 0)
            {
                ok = true;
            }
            else
            {
                if (errno != EINPROGRESS)
                {
                    log(LOG_ERR, "connection to SDA failed");
                }
                else
                {
                    fd_set fds;
                    struct timeval tv = {1, 0};
                    FD_ZERO(&fds);
                    FD_SET(m_sockfd, &fds);
                    int selVal = ::select(m_sockfd+1, nullptr, &fds, nullptr, &tv);
                    if (selVal == -1)
                    {
                        log(LOG_ERR, "sockfd not ready\n");
                        close(m_sockfd);
                    }
                    else if (selVal == 0)
                    {
                        log(LOG_ERR, "socket timeout");
                        close(m_sockfd);
                    }
                    else
                    {
                        ok = true;
                    }
                }
            }
        }
        else
        {
            log(LOG_ERR, "could not create socket");
        }

        if (ok)
        {
            log(LOG_INFO, "connected to SDA");
            m_state = state::Connected;
        }

        return ok;
    }

    void Sapient::disconnect()
    {
        if (m_sockfd >= 0)
        {
            close(m_sockfd);
        }
        m_sockfd = -1;
        m_state = state::NotConnected;
    }

    void Sapient::sendMessage(SapientMessage &msg)
    {
        char sendBuff[32 * 1024] = {0};
        msg.serialise(sendBuff);
        // Send string length + 1 so that we send null terminator
        if (::write(m_sockfd, sendBuff, strlen(sendBuff) + 1) == -1)
        {
            m_state = state::NotConnected;
        }
    }

    char *Sapient::getIpString(sockaddr *sa, char *s, size_t maxlen)
    {
        // Convert a struct sockaddr address to a string, IPv4 and IPv6:
        switch(sa->sa_family)
        {
            case AF_INET:
                inet_ntop(AF_INET, &((reinterpret_cast<sockaddr_in *>(sa))->sin_addr), s, maxlen);
                break;

            case AF_INET6:
                inet_ntop(AF_INET6, &((reinterpret_cast<sockaddr_in6 *>(sa))->sin6_addr), s, maxlen);
                break;

            default:
                strncpy(s, "Unknown AF", maxlen);
                s = nullptr;
        }

        return s;
    }
}

#if defined(RAPIDXML_NO_EXCEPTIONS)
void rapidxml::parse_error_handler(const char* what, void* where) {
    log(LOG_ERR, "Parse error(@%p): %s", where, what);
    log(LOG_INFO, "Aborting as XML parser does not currently support badly formatted XML");
    std::abort();
}
#endif
