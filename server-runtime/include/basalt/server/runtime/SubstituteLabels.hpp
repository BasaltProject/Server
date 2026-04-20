#pragma once

#include <basalt/wire/multiversion/SubstitutionLabels.hpp>

#include <cstdint>
#include <string>

namespace basalt::server::runtime {

class SubstituteLabels final : public wire::multiversion::SubstitutionLabels {
public:
    explicit SubstituteLabels(std::string version);

    SubstituteLabels();

    [[nodiscard]] std::optional<wire::text::Component> itemLabel(std::int32_t nativeItemId) const override;

    [[nodiscard]] std::optional<wire::text::Component> entityLabel(std::int32_t nativeEntityTypeId) const override;

private:
    std::string version_;
};

} // namespace basalt::server::runtime
