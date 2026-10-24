package io.papermc.paper.world.flag;

import java.util.Set;
import org.bukkit.FeatureFlag;

/** Everything the server offers is part of the game as shipped, so no feature flag is ever required beyond it. */
public final class BasaltFeatureFlagProvider implements FeatureFlagProvider {
    @Override
    public Set<FeatureFlag> requiredFeatures(FeatureDependant dependant) {
        return Set.of(FeatureFlag.VANILLA);
    }
}
