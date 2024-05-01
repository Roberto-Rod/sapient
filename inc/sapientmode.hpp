/*
 * SendMission.hpp
 *
 *  Created on: 11 Jan 2020
 *      Author: rh
 */

#ifndef SRC_SAPIENTMODE_HPP_
#define SRC_SAPIENTMODE_HPP_

#include <cstdint>
#include <string>
#include <atomic>
#include <chrono>

namespace sapient
{
    class SapientMode
    {
    public:
        static SapientMode &instance();

        void setMode(int32_t mode);
        int32_t mode();
        bool getMissionName(uint32_t mode, std::string &name) const;
        bool getMissionFileName(uint32_t mode, std::string &name) const;
        bool getMissionName(std::string &name) const;
        bool doMissionFilesExist() const;

    private:
        SapientMode(){}
        virtual ~SapientMode() {}

        const std::string kMissionPrefix {"KT-956-0185-00"};
        const std::string kMissionSuffix {".iff"};
        const std::string kMissionFileLocation {"missions/"};
        static const uint32_t kModeAccumulationTime_ms = 1000;
        std::chrono::time_point<std::chrono::steady_clock> m_modeSetTime;
        std::atomic<uint32_t> m_mode {0};
        std::atomic<uint32_t> m_latchedMode {std::numeric_limits<uint32_t>::max()};
    };
}
#endif //SRC_SAPIENTMODE_HPP
