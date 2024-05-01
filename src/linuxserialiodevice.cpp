#include "linuxserialiodevice.hpp"
#include "base/baselib/inc/circbuffer.hpp"
#include "board/boardlib/inc/system.hpp"
#include "debuglog.hpp"

#include <errno.h>
#include <fcntl.h>
#include <sys/signal.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

namespace mercury
{
    namespace embedded
    {
        namespace sio
        {
            // getSerialIoDevice by UART number is needed to satisfy the sio library.
            // If this function is called then assume device is a ttyUSB adapter
            SerialIoDevice* getSerialIoDevice(int32_t uartNumber)
            {
                std::string node("/dev/ttyUSB");
                node += std::to_string(uartNumber);
                return getSerialIoDevice(node);
            }

            SerialIoDevice* getSerialIoDevice(std::string const &node)
            {
                static LinuxSerialIoDevice instance(node);

                return &instance;
            }

            LinuxSerialIoDevice::LinuxSerialIoDevice(std::string const &node)
                : m_node(node),
                  m_fd(-1),
                  m_isGood(false)
            {
                reinitialise();
            }

            LinuxSerialIoDevice::~LinuxSerialIoDevice()
            {
                flush();
                deinitialise();
            }

            int32_t LinuxSerialIoDevice::writeRaw(const uint8_t* pData, int32_t numberOfBytes)
            {
                int32_t bytesWritten = ::write(m_fd, pData, numberOfBytes);
                m_isGood = (bytesWritten == numberOfBytes);
                ::tcdrain(m_fd);

                if (!m_isGood)
                {
                    log(LOG_ERR, "serial write error (%d / %d)", bytesWritten, numberOfBytes);
                }

                #ifdef SERIAL_DEBUG
                log(LOG_INFO, "serial IO device sent %d bytes", bytesWritten);
                #endif

                return m_isGood ? bytesWritten : 0;
            }

            bool LinuxSerialIoDevice::isGood()
            {
                return m_isGood;
            }

            void LinuxSerialIoDevice::deinitialise()
            {
                ::close(m_fd);
                if (m_fd >= 0)
                {
                    log(LOG_INFO, "closed %s", m_node.c_str());
                }
                m_fd = -1;
            }

            void LinuxSerialIoDevice::reinitialise(int32_t baud)
            {
                m_isGood = false;
                deinitialise();
                m_fd = ::open(m_node.c_str(), O_RDWR | O_NOCTTY | O_NONBLOCK);
                if (m_fd < 0)
                {
                    log(LOG_ERR, "failed to open %s", m_node.c_str());
                }
                else
                {
                    m_isGood = installSignalHandler() && setInterfaceAttribs(m_fd, B115200, 0);
                    if (m_isGood)
                    {
                        log(LOG_INFO, "opened %s", m_node.c_str());
                    }
                    else
                    {
                        log(LOG_ERR, "opened but failed to configure%s", m_node.c_str());
                    }
                }
            }

            void LinuxSerialIoDevice::read()
            {
                char buf[255];
                int n(::read(m_fd, buf, sizeof(buf)));

                #ifdef SERIAL_DEBUG
                if (n > 0)
                {
                    log(LOG_INFO, "serial IO device received %d bytes", n);
                }
                #endif

                for (int i = 0; i < n; ++i)
                {
                    SerialIoDevice::insert(buf[i]);
                }
            }

            bool LinuxSerialIoDevice::installSignalHandler()
            {
                bool ok(true);

                struct sigaction saio{0};
                saio.sa_handler = signalHandler;
                ok = ok && (sigaction(SIGIO, &saio, nullptr) == 0);

                // Allow the process to receive SIGIO
                ok = ok && (::fcntl(m_fd, F_SETOWN, getpid()) == 0);

                // Make the file descriptor asynchronous
                ok = ok && (::fcntl(m_fd, F_SETFL, FASYNC) == 0);

                return ok;
            }

            void LinuxSerialIoDevice::signalHandler(int status)
            {
            }

            bool LinuxSerialIoDevice::setInterfaceAttribs(int fd, int speed, bool parity)
            {
                bool ok(false);
                termios tty;

                ok = (tcgetattr(fd, &tty) == 0);

                if (!ok)
                {
                    log(LOG_ERR, "tcgetattr");
                }
                else
                {
                    cfsetospeed (&tty, speed);
                    cfsetispeed (&tty, speed);

                    tty.c_cflag = (tty.c_cflag & ~CSIZE) | CS8;     // 8-bit chars
                    // disable IGNBRK for mismatched speed tests; otherwise receive break
                    // as \000 chars
                    tty.c_iflag =0;                         // no break processing, no input remapping, no xon/xoff
                    tty.c_lflag = 0;                        // no signaling chars, no echo,
                                                            // no canonical processing
                    tty.c_oflag = 0;                        // no remapping, no delays
                    tty.c_cc[VMIN]  = 0;                    // no read blocking
                    tty.c_cc[VTIME] = 0;                    // 0.0 seconds read timeout

                    tty.c_cflag |= (CLOCAL | CREAD);        // ignore modem controls,
                                                            // enable reading
                    tty.c_cflag &= ~(PARENB | PARODD);      // disable parity
                    tty.c_cflag |= parity ? 1 : 0;
                    tty.c_cflag &= ~CSTOPB;
                    tty.c_cflag &= ~CRTSCTS;

                    ok = (tcsetattr (fd, TCSANOW, &tty) == 0);
                    if (!ok)
                    {
                        log(LOG_ERR, "tcsetattr");
                    }
                }
                return ok;
            }
        }
    }
}
