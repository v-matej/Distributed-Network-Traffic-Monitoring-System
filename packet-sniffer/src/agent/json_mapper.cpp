#include "agent/json_mapper.hpp"

#include <nlohmann/json.hpp>

namespace agent {

using json = nlohmann::json;

std::string to_json(const HealthInfo& health) {
    json j;
    j["status"] = health.status;
    j["agent_name"] = health.agent_name;
    j["version"] = health.version;
    j["hostname"] = health.hostname;
    return j.dump(4);
}

std::string to_json(const std::vector<sniffer::InterfaceInfo>& interfaces) {
    json j;
    j["interfaces"] = json::array();

    for (const auto& iface : interfaces) {
        j["interfaces"].push_back({
            {"name", iface.name},
            {"description", iface.description}
        });
    }

    return j.dump(4);
}

}  // namespace agent