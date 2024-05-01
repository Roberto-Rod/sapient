#include "board.hpp"

#include <sys/time.h>

namespace mercury
{
    namespace embedded
    {
        namespace board
        {
            uint32_t systemTimeMs()
            {
                timeval now;
                gettimeofday(&now, nullptr);
                return (static_cast<uint32_t>(now.tv_sec) * 1000) + (static_cast<uint32_t>(now.tv_usec)/1000);
            }
        }
    }
}
