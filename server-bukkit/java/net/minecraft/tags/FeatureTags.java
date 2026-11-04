// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.tags;

import net.minecraft.core.registries.Registries;
import net.minecraft.resources.Identifier;
import net.minecraft.world.level.levelgen.feature.Feature;

public final class FeatureTags {
    public static final TagKey<Feature> CAN_SPAWN_FROM_BONE_MEAL = org.basaltmc.bukkit.facade.Tags.key(Registries.FEATURE, "can_spawn_from_bone_meal");

    private FeatureTags() {}
}
