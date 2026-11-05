// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.flag;

import java.util.Set;
import net.minecraft.core.Registry;
import net.minecraft.resources.ResourceKey;
public interface FeatureElement {
    @SuppressWarnings({"unchecked", "rawtypes"})
    Set<ResourceKey<? extends Registry<? extends FeatureElement>>> FILTERED_REGISTRIES = (Set) Set.of(
        net.minecraft.core.registries.Registries.ITEM, net.minecraft.core.registries.Registries.BLOCK,
        net.minecraft.core.registries.Registries.ENTITY_TYPE, net.minecraft.core.registries.Registries.GAME_RULE,
        net.minecraft.core.registries.Registries.MENU, net.minecraft.core.registries.Registries.POTION,
        net.minecraft.core.registries.Registries.MOB_EFFECT);
    FeatureFlagSet requiredFeatures();
    default boolean isEnabled(FeatureFlagSet enabledFeatures) {
        return requiredFeatures().isSubsetOf(enabledFeatures);
    }
}
