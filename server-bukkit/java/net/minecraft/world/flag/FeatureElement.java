// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.flag;

import java.util.Set;
import net.minecraft.core.Registry;
import net.minecraft.resources.ResourceKey;
public interface FeatureElement {
    Set<ResourceKey<? extends Registry<? extends FeatureElement>>> FILTERED_REGISTRIES = null;
    default FeatureFlagSet requiredFeatures() {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.flag.FeatureElement#requiredFeatures");
    }
    default boolean isEnabled(FeatureFlagSet enabledFeatures) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.flag.FeatureElement#isEnabled");
    }
}
