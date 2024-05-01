#include <thread>
#include <cstdio>
#include <cinttypes>
#include <cstring>

#include "debuglog.hpp"
#include "mercury.hpp"
#include "sapient.hpp"
#include "sapientmode.hpp"
#include "version.hpp"

int main(int argc, char *argv[])
{
    printf("SAPIENT Mediator (KT-956-0186-00) Version: %s\n\n", sapient::kVersionString.c_str());
    if (argc < 2)
    {
        printf("Usage: %s <server-ip> [<server-port>] [<serial-dev>]\ne.g.   %s 127.0.0.1 14006 /dev/ttyUSB0\n\n",argv[0], argv[0]);
    }
    else
    {
        std::string serialPort("/dev/ttyUSB0");
        uint16_t serverPort(14006);
        std::string ipAddress(argv[1]);
        bool debugTerminator(false);

        if (argc >= 3)
        {
            sscanf(argv[2], "%" SCNd16, &serverPort);
        }
        if (argc >= 4)
        {
            serialPort = argv[3];
        }
        if (argc >= 5)
        {
            if (::strcmp(argv[4], "-d") == 0)
            {
                debugTerminator = true;
                log(LOG_INFO, "using debug terminator");
            }
        }

        openlog("sapient", 0, 0);

        // Check mission files exist, called function logs warnings if not
        (void)sapient::SapientMode::instance().doMissionFilesExist();

        // Start threads
        std::thread threadMercury(sapient::Mercury(), serialPort);
        std::thread threadSapient(sapient::Sapient(), ipAddress, serverPort, debugTerminator);

        threadMercury.join();
        threadSapient.join();
        log(LOG_INFO, "exiting");
    }
}

