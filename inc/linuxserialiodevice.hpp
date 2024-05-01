#ifndef LINUXSERIALIODEVICE_HPP_
#define LINUXSERIALIODEVICE_HPP_

#include "sio/siolib/inc/serialiodevice.hpp"

namespace mercury
{
    namespace embedded
    {
        namespace sio
        {
            /// @param node Serial port device node e.g. /dev/ttyUSB0
            SerialIoDevice* getSerialIoDevice(std::string const &node);

            /// instantiable serial io device class for use on Linux OS
            class LinuxSerialIoDevice : public SerialIoDevice
            {
            public:
                ~LinuxSerialIoDevice();

                int32_t writeRaw(const uint8_t* pData, int32_t numberOfBytes);

                bool isGood();

                void deinitialise();

                void reinitialise(int32_t baud = 115200);

                void read();

            private:
                LinuxSerialIoDevice(std::string const &node);
                bool installSignalHandler();
                static void signalHandler(int status);
                static bool setInterfaceAttribs(int fd, int speed, bool parity);
                friend SerialIoDevice* getSerialIoDevice(std::string const &node);

                std::string m_node;
                int m_fd;
                bool m_isGood;
            };
        }
    }
}
#endif // LINUXSERIALIODEVICE_HPP_

