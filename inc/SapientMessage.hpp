#ifndef SAPIENT_MESSAGE_HPP
#define SAPIENT_MESSAGE_HPP

#include <string>
#include <vector>
#include <memory>
// RapidXML includes
#include <cstddef>
#include <cassert>
#include <iostream>
#include <sstream>
#include <rapidxml_utils.hpp>
#include <rapidxml_print.hpp>
#include <rapidxml.hpp>
#include <cstddef>
#include <cassert>
#include <string>
#include <vector>
#include <fstream>
#include <fcntl.h>
#include <cstdint>

class SapientMessage;

std::shared_ptr<SapientMessage> sapientMessageFactory(const char* buffer);

class SapientMessage
{
public:
    SapientMessage();
    virtual ~SapientMessage() {}
    
    void initialise();
    void parse(const char* buffer);
    void walk(const rapidxml::xml_node<>* node, int indent = 0);
    virtual bool serialise(char* buffer);
    virtual bool deserialise(const char* buffer);
    
    rapidxml::xml_document<> m_doc;
protected:
    std::vector<std::string> m_nodes;
    std::vector<std::string> m_keys;
    std::vector<std::string> m_values;
};

class SapientMessageSensorRegistration : public SapientMessage
{
public:
    SapientMessageSensorRegistration() {}
    ~SapientMessageSensorRegistration() {}
    
    bool serialise(char* buffer);
    bool deserialise(const char* buffer) { return false; }
    
    std::string m_sensorType{"Sky Net Longbow"};
    int32_t m_sensorId {0};
    bool m_sensorIdSet {false};
    std::string m_heartbeatUnits;
    int32_t m_heartbeatValue;
    std::string m_modeType;
    std::string m_modeName;
    std::string m_settleTimeUnits;
    int32_t m_settleTimeValue;
    std::string m_locationType;
    std::string m_locationTypeUnits;
    std::string m_locationTypeDatum;
    std::string m_locationTypeZone;
    std::string m_locationTypeNorth;
};

class SapientMessageSensorRegistrationAck : public SapientMessage
{
public:
    SapientMessageSensorRegistrationAck() {}
    ~SapientMessageSensorRegistrationAck() {}
    
    bool serialise(char* buffer) { return false; }
    bool deserialise(const char* buffer);

    int32_t m_sensorId {0};
};

class SapientMessageHeartbeat : public SapientMessage
{
public:
    SapientMessageHeartbeat() {}
    ~SapientMessageHeartbeat() {}

    bool serialise(char* buffer);
    bool deserialise(const char* buffer) { return false; }

    int32_t m_sensorId {0};
    int32_t m_reportId {0};
    std::string m_system {"OK"};
    bool m_changed {false};
};

class SapientMessageSensorTask : public SapientMessage
{
public:
    SapientMessageSensorTask() {}
    ~SapientMessageSensorTask() {}

    bool serialise(char* buffer) { return false; }
    bool deserialise(const char* buffer);

    int32_t m_sensorId {0};
    int32_t m_taskId {0};
    std::string m_control {""};
    std::string m_request {""};
    int32_t m_mode {0};
    bool m_changed {false};
};

#endif //SAPIENT_MESSAGE_HPP
