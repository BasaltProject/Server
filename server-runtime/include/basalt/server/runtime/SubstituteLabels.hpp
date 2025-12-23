#pragma once

#include <basalt/wire/multiversion/SubstitutionLabels.hpp>

#include <string>

namespace basalt::server::runtime {

/// Names the stand-in sent for content a client is too old for, version included: `Cinnabar (26.2)`.
class SubstituteLabels final : public wire::multiversion::SubstitutionLabels {
public:
    explicit SubstituteLabels(std::string version);

    /// The build's own target version, which is what every caller but a test wants.
    SubstituteLabels();

    [[nodiscard]] std::optional<wire::text::Component> itemLabel(std::int32_t nativeItemId) const override;

    [[nodiscard]] std::optional<wire::text::Component> entityLabel(std::int32_t nativeEntityTypeId) const override;

private:
    std::string version_;
};

} // namespace basalt::server::runtime
