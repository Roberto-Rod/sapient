#include "SapientMessage.hpp"
#include "debuglog.hpp"

#include <ctime>
#include <algorithm>

std::shared_ptr<SapientMessage> sapientMessageFactory(const char* buffer)
{
    std::shared_ptr<SapientMessage> ptr(nullptr);
    SapientMessage msg;
    msg.parse(buffer);
    
    std::string node(msg.m_doc.first_node()->name(), msg.m_doc.first_node()->name_size());
    
    if (node == "SensorRegistrationACK")
    {
        ptr = std::make_shared<SapientMessageSensorRegistrationAck>();
    }
    else if (node == "SensorTask")
    {
        ptr = std::make_shared<SapientMessageSensorTask>();
    }
    
    if (ptr)
    {
        ptr->deserialise(buffer);
    }
    
    return ptr;
}

SapientMessage::SapientMessage()
{
    initialise();
}

void SapientMessage::initialise()
{
    m_doc.clear();
    
    // xml declaration
    rapidxml::xml_node<>* decl = m_doc.allocate_node(rapidxml::node_declaration);
    decl->append_attribute(m_doc.allocate_attribute("version", "1.0"));
    decl->append_attribute(m_doc.allocate_attribute("encoding", "utf-8"));
    m_doc.append_node(decl);
}

void SapientMessage::parse(const char* buffer)
{
    m_nodes.clear();
    m_keys.clear();
    m_values.clear();
    m_doc.clear();
    // NOTE : There is a `const_cast<>`, but `rapidxml::parse_non_destructive`
    //        guarantees `data` is not overwritten.
    m_doc.parse<rapidxml::parse_non_destructive>(const_cast<char*>(buffer));
}

void SapientMessage::walk(const rapidxml::xml_node<>* node, int indent)
{
    //const auto ind = std::string(indent * 4, ' ');
    //printf("%s", ind.c_str());

    std::string fullNode, data;
    const rapidxml::node_type t = node->type();
    switch(t)
    {
        case rapidxml::node_element:
        {
            //printf("%.*s", int(node->name_size()), node->name());
            if ((indent + 1) > int(m_nodes.size()))
            {
                m_nodes.push_back(std::string(node->name(), int(node->name_size())));
            }
            else
            {
                m_nodes.at(indent) = std::string(node->name(), int(node->name_size()));
            }
            
            for(const rapidxml::xml_attribute<>* a = node->first_attribute(); a; a = a->next_attribute())
            {
                //printf(" %.*s", int(a->name_size()), a->name());
                //printf("='%.*s'", int(a->value_size()), a->value());
            }
            //printf(":\n");
            for(const rapidxml::xml_node<>* n = node->first_node(); n; n = n->next_sibling())
            {
                walk(n, indent+1);
            }
            //printf("%s</%.*s>\n", ind.c_str(), int(node->name_size()), node->name());
        }
        break;

    case rapidxml::node_data:     
        fullNode.clear();
        for (auto const& value: m_nodes)
        {
            if (fullNode.size())
            {
                fullNode += ".";
            }
            fullNode += value;
        }
        data = std::string(node->value(), int(node->value_size()));
        m_keys.push_back(fullNode);
        m_values.push_back(data);
        //printf("%s = %s\n", fullNode.c_str(), data.c_str());
        break;

    default:
        //printf("NODE-TYPE:%d\n", t);
        break;
    }
}

bool SapientMessage::serialise(char* buffer)
{
    char *end = rapidxml::print(buffer, m_doc, 0); // end contains pointer to character after last printed character
    *(end--) = 0;                                  // Add string terminator after XML

    // Replace new lines at end of string with null (this suppresses a non-critical message in the SAPIENT middleware)
    while (*end == '\r' || *end == '\n')
    {
        *(end--) = 0;
    }

    return true;
}

bool SapientMessage::deserialise(const char* buffer)
{    
    bool result(false);
    
    parse(buffer);
    walk(m_doc.first_node());
    
    return result;
}

// *** SapientMessageSensorRegistration *** //
bool SapientMessageSensorRegistration::serialise(char* buffer)
{
    bool result(false);
    
    initialise();
    
    // Root node
    rapidxml::xml_node<>* root = m_doc.allocate_node(rapidxml::node_element, "SensorRegistration");    

    // Timestamp node
    char timeBuf[21];
    time_t now;
    time(&now);
    strftime(timeBuf, sizeof(timeBuf), "%FT%TZ", gmtime(&now));
    rapidxml::xml_node<>* ts = m_doc.allocate_node(rapidxml::node_element, "timestamp", timeBuf);
    
    // Sensor ID node
    std::string sensorId(std::to_string(m_sensorId));
    rapidxml::xml_node<>* sid = m_doc.allocate_node(rapidxml::node_element, "sensorID", sensorId.c_str());
    
    // Sensor type node
    rapidxml::xml_node<>* st = m_doc.allocate_node(rapidxml::node_element, "sensorType", m_sensorType.c_str());
    
    // Heartbeat definition node
    rapidxml::xml_node<>* hd = m_doc.allocate_node(rapidxml::node_element, "heartbeatDefinition");
    rapidxml::xml_node<>* hi = m_doc.allocate_node(rapidxml::node_element, "heartbeatInterval");
    hi->append_attribute(m_doc.allocate_attribute("units", "seconds"));
    hi->append_attribute(m_doc.allocate_attribute("value", "10"));
    hd->append_node(hi);
    
    // Mode definition node(s)
    rapidxml::xml_node<>* mdd = m_doc.allocate_node(rapidxml::node_element, "modeDefinition");
    rapidxml::xml_node<>* mdj = m_doc.allocate_node(rapidxml::node_element, "modeDefinition");
    rapidxml::xml_node<>* mnd = m_doc.allocate_node(rapidxml::node_element, "modeName", "Default");
    rapidxml::xml_node<>* mnj = m_doc.allocate_node(rapidxml::node_element, "modeName", "jam");
    rapidxml::xml_node<>* stld = m_doc.allocate_node(rapidxml::node_element, "settleTime");
    rapidxml::xml_node<>* stlj = m_doc.allocate_node(rapidxml::node_element, "settleTime");
    rapidxml::xml_node<>* mpj = m_doc.allocate_node(rapidxml::node_element, "modeParameter");
    rapidxml::xml_node<>* ddd = m_doc.allocate_node(rapidxml::node_element, "detectionDefinition");
    rapidxml::xml_node<>* ddj = m_doc.allocate_node(rapidxml::node_element, "detectionDefinition");
    rapidxml::xml_node<>* ltd = m_doc.allocate_node(rapidxml::node_element, "locationType", "GPS");
    rapidxml::xml_node<>* ltj = m_doc.allocate_node(rapidxml::node_element, "locationType", "GPS");
    rapidxml::xml_node<>* tdd = m_doc.allocate_node(rapidxml::node_element, "taskDefinition");
    rapidxml::xml_node<>* tdj = m_doc.allocate_node(rapidxml::node_element, "taskDefinition");
    stld->append_attribute(m_doc.allocate_attribute("units", "seconds"));
    stld->append_attribute(m_doc.allocate_attribute("value", "10"));
    stlj->append_attribute(m_doc.allocate_attribute("units", "seconds"));
    stlj->append_attribute(m_doc.allocate_attribute("value", "10"));
    mpj->append_attribute(m_doc.allocate_attribute("type", "Frequency Band"));
    mpj->append_attribute(m_doc.allocate_attribute("value", "Required"));
    ltd->append_attribute(m_doc.allocate_attribute("units", "decimal degrees-metres"));
    ltd->append_attribute(m_doc.allocate_attribute("datum", "WGS84"));
    ltd->append_attribute(m_doc.allocate_attribute("zone", "30U"));
    ltd->append_attribute(m_doc.allocate_attribute("north", "Grid"));
    ltj->append_attribute(m_doc.allocate_attribute("units", "decimal degrees-metres"));
    ltj->append_attribute(m_doc.allocate_attribute("datum", "WGS84"));
    ltj->append_attribute(m_doc.allocate_attribute("zone", "30U"));
    ltj->append_attribute(m_doc.allocate_attribute("north", "Grid"));
    ddd->append_node(ltd);
    ddj->append_node(ltj);
    mdd->append_attribute(m_doc.allocate_attribute("type", "Permanent"));
    mdd->append_node(mnd);
    mdd->append_node(stld);
    mdd->append_node(ddd);
    mdd->append_node(tdd);
    mdj->append_attribute(m_doc.allocate_attribute("type", "Permanent"));
    mdj->append_node(mnj);
    mdj->append_node(stlj);
    mdj->append_node(ddj);
    mdj->append_node(tdj);
    
    // Assemble nodes
    m_doc.append_node(root);
    root->append_node(ts);
    if (m_sensorIdSet)
    {
        root->append_node(sid);
    }
    root->append_node(st);
    root->append_node(hd);
    root->append_node(mdd);
    root->append_node(mdj);
    
    // Generate output string
    return SapientMessage::serialise(buffer) && result;
}

// *** SapientMessageHeartbeat *** //
bool SapientMessageHeartbeat::serialise(char *buffer)
{
    bool result(false);

    initialise();

    // Root node
    rapidxml::xml_node<>* root = m_doc.allocate_node(rapidxml::node_element, "StatusReport");

    // Timestamp node
    char timeBuf[21];
    time_t now;
    time(&now);
    strftime(timeBuf, sizeof(timeBuf), "%FT%TZ", gmtime(&now));
    rapidxml::xml_node<>* ts = m_doc.allocate_node(rapidxml::node_element, "timestamp", timeBuf);

    // Sensor ID node
    std::string sensorId(std::to_string(m_sensorId));
    rapidxml::xml_node<>* sid = m_doc.allocate_node(rapidxml::node_element, "sourceID", sensorId.c_str());

    // Report ID node
    std::string reportId(std::to_string(m_reportId));
    rapidxml::xml_node<>* rid = m_doc.allocate_node(rapidxml::node_element, "reportID", reportId.c_str());

    // System node
    rapidxml::xml_node<>* sys = m_doc.allocate_node(rapidxml::node_element, "system", m_system.c_str());

    // Info node
    std::string info("Unchanged");
    if (m_reportId == 0)
    {
        info = "New";
    }
    else if (m_changed)
    {
        info = "Additional";
    }
    rapidxml::xml_node<>* inf = m_doc.allocate_node(rapidxml::node_element, "info", info.c_str());

    // Assemble nodes
    m_doc.append_node(root);
    root->append_node(ts);
    root->append_node(sid);
    root->append_node(rid);
    root->append_node(sys);
    root->append_node(inf);

    // Generate output string
    return SapientMessage::serialise(buffer) && result;
}

// *** SapientMessageSensorRegistrationAck *** //
bool SapientMessageSensorRegistrationAck::deserialise(const char* buffer)
{
    bool result(false);
    
    SapientMessage::deserialise(buffer);
    
    // Have we got the expected key?
    std::vector<std::string>::iterator it = std::find(m_keys.begin(), m_keys.end(), "SensorRegistrationACK.sensorID");
    if (it != m_keys.end())
    {
        int index = std::distance(m_keys.begin(), it);
        m_sensorId = atoi(m_values.at(index).c_str());
        result = true;
    }
    
    return result;
}

bool SapientMessageSensorTask::deserialise(const char* buffer)
{
    // TODO: control overall result as we work through the keys
    bool result(false);

    SapientMessage::deserialise(buffer);

    // Have we got the expected sensorID key?
    std::vector<std::string>::iterator it = std::find(m_keys.begin(), m_keys.end(), "SensorTask.sensorID");
    if (it != m_keys.end())
    {
        int index = std::distance(m_keys.begin(), it);
        m_sensorId = atoi(m_values.at(index).c_str());
        result = true;
    }

    // Have we got the expected taskID key?
    it = std::find(m_keys.begin(), m_keys.end(), "SensorTask.taskID");
    if (it != m_keys.end())
    {
        int index = std::distance(m_keys.begin(), it);
        m_taskId = atoi(m_values.at(index).c_str());
        result = true;
    }

    // Have we got the expected control key?
    it = std::find(m_keys.begin(), m_keys.end(), "SensorTask.control");
    if (it != m_keys.end())
    {
        int index = std::distance(m_keys.begin(), it);
        m_control = m_values.at(index);
        result = true;
    }

    // Have we got the expected request key?
    it = std::find(m_keys.begin(), m_keys.end(), "SensorTask.command.request");
    if (it != m_keys.end())
    {
        int index = std::distance(m_keys.begin(), it);
        m_request = m_values.at(index);
        result = true;
    }

    // Have we got the expected mode key?
    it = std::find(m_keys.begin(), m_keys.end(), "SensorTask.command.mode");
    if (it != m_keys.end())
    {
        int index = std::distance(m_keys.begin(), it);
        if (m_values.at(index).substr(0, 4) == "jam ")
        {
            m_mode = atoi(m_values.at(index).substr(4).c_str());
            result = true;
        }
    }

    return result;
}

