// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.tags;

import net.minecraft.core.registries.Registries;
import net.minecraft.resources.Identifier;
import net.minecraft.world.level.levelgen.presets.WorldPreset;

public final class WorldPresetTags {
    public static final TagKey<WorldPreset> NORMAL = org.basaltmc.bukkit.facade.Tags.key(Registries.WORLD_PRESET, "normal");
    public static final TagKey<WorldPreset> EXTENDED = org.basaltmc.bukkit.facade.Tags.key(Registries.WORLD_PRESET, "extended");

    private WorldPresetTags() {}
}
