#include "sapientmode.hpp"
#include "debuglog.hpp"

#include <chrono>
#include <unistd.h>

namespace sapient
{
    SapientMode &SapientMode::instance()
    {
        static SapientMode s;
        return s;
    }

    void SapientMode::setMode(int32_t mode)
    {
        // If mode is 0 then clear overall mode
        if (mode == 0)
        {
            m_mode = 0;
        }
        else
        {
            // Only decode "wideband, omni" modes for now
            if (mode <= 7)
            {
                m_mode |= 1 << (mode - 1);
            }
        }

        m_modeSetTime = std::chrono::steady_clock::now();
    }

    int32_t SapientMode::mode()
    {
        auto now(std::chrono::steady_clock::now());
        if (std::chrono::duration_cast<std::chrono::milliseconds>(now - m_modeSetTime).count() >= kModeAccumulationTime_ms)
        {
            if (m_mode != m_latchedMode)
            {
                log(LOG_INFO, "changing composite mode to %u", uint32_t(m_mode));
            }
            m_latchedMode = uint32_t(m_mode);
        }
        return m_latchedMode;
    }

    bool SapientMode::getMissionName(std::string &name) const
    {
        return getMissionName(m_mode, name);
    }

    bool SapientMode::getMissionFileName(uint32_t mode, std::string &name) const
    {
        bool ok(getMissionName(mode, name));
        name = kMissionFileLocation + name + kMissionSuffix;
        return ok;
    }

    bool SapientMode::getMissionName(uint32_t mode, std::string &name) const
    {
        bool ok(true);
        uint8_t ecm[5];
        ecm[0] = (mode & 0x02) >> 1;
        ecm[1] = (mode & 0x0C) >> 2;
        ecm[2] = ((mode & 0x10) >> 3) | (mode & 0x01);
        ecm[3] = (mode & 0x20) >> 5;
        ecm[4] = (mode & 0x40) >> 6;

        name = kMissionPrefix;

        // ECM 1
        name += ecm[0] ? "_AB" : "_AA";

        // ECM 2
        if (ecm[1] == 0)
        {
            name += "_AAA";
        }
        else if (ecm[1] == 1)
        {
            name += "_AAB";
        }
        else if (ecm[1] == 2)
        {
            name += "_AAC";
        }
        else if (ecm[1] == 3)
        {
            name += "_ABC";
        }
        else
        {
            name += "_???";
        }

        // ECM 3
        if (ecm[2] == 0)
        {
            name += "_AA";
        }
        else if (ecm[2] == 1)
        {
            name += "_AC";
        }
        else if (ecm[2] == 2)
        {
            name += "_AB";
        }
        else if (ecm[2] == 3)
        {
            name += "_BC";
        }
        else
        {
            name += "_??";
        }

        // ECM 4
        name += ecm[3] ? "_AB" : "_AA";

        // ECM 5
        name += ecm[4] ? "_AB" : "_AA";

        return ok;
    }

    bool SapientMode::doMissionFilesExist() const
    {
        bool ok(true);

        for (uint32_t mode = 0; mode <= 127; ++mode)
        {
            std::string name;
            if (getMissionFileName(mode, name))
            {
                if (::access(name.c_str(), F_OK) == -1)
                {
                    log(LOG_WARNING, "file not found: %s (mode 0x%02x)", name.c_str(), mode);
                    ok = false;
                }
            }
            else
            {
                log(LOG_ERR, "getMissionName(%u) failed", mode);
            }
        }

        return ok;
    }
}
