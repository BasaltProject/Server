// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.tags;

import net.minecraft.core.registries.Registries;
import net.minecraft.resources.Identifier;
import net.minecraft.world.level.levelgen.flat.FlatLevelGeneratorPreset;

public final class FlatLevelGeneratorPresetTags {
    public static final TagKey<FlatLevelGeneratorPreset> VISIBLE = org.basaltmc.bukkit.facade.Tags.key(Registries.FLAT_LEVEL_GENERATOR_PRESET, "visible");

    private FlatLevelGeneratorPresetTags() {}
}
