#include <basalt/server/runtime/BasaltServer.hpp>

#include <algorithm>

namespace basalt::server::runtime {

std::vector<std::string> BasaltServer::enabledFeatures() const {
    std::vector<std::string> features;
    if (options_.datapacks != nullptr) {
        for (const wire::common::Identifier& flag : options_.datapacks->configuration().enabledFeatures) {
            features.push_back(flag.toString());
        }
    }
    if (features.empty()) {
        features.emplace_back("minecraft:vanilla");
    }
    return features;
}

bool BasaltServer::minecartImprovements() const {
    return options_.datapacks != nullptr &&
           std::ranges::contains(options_.datapacks->configuration().enabledFeatures,
                                 wire::common::Identifier::vanilla("minecart_improvements"));
}

} // namespace basalt::server::runtime
