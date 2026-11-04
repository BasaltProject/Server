// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.tags;

import net.minecraft.core.registries.Registries;
import net.minecraft.resources.Identifier;
import net.minecraft.world.timeline.Timeline;

public interface TimelineTags {
    TagKey<Timeline> UNIVERSAL = org.basaltmc.bukkit.facade.Tags.key(Registries.TIMELINE, "universal");
    TagKey<Timeline> IN_OVERWORLD = org.basaltmc.bukkit.facade.Tags.key(Registries.TIMELINE, "in_overworld");
    TagKey<Timeline> IN_NETHER = org.basaltmc.bukkit.facade.Tags.key(Registries.TIMELINE, "in_nether");
    TagKey<Timeline> IN_END = org.basaltmc.bukkit.facade.Tags.key(Registries.TIMELINE, "in_end");
}
