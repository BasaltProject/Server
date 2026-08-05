package io.papermc.paper.world.flag;

import java.util.Set;
import org.bukkit.FeatureFlag;

public final class BasaltFeatureFlagProvider implements FeatureFlagProvider {
    @Override
    public Set<FeatureFlag> requiredFeatures(FeatureDependant dependant) {
        return Set.of(FeatureFlag.VANILLA);
    }
}
