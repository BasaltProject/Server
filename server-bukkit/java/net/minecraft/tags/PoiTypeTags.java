// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.tags;

import net.minecraft.core.registries.Registries;
import net.minecraft.resources.Identifier;
import net.minecraft.world.entity.ai.village.poi.PoiType;

public final class PoiTypeTags {
    public static final TagKey<PoiType> ACQUIRABLE_JOB_SITE = org.basaltmc.bukkit.facade.Tags.key(Registries.POINT_OF_INTEREST_TYPE, "acquirable_job_site");
    public static final TagKey<PoiType> VILLAGE = org.basaltmc.bukkit.facade.Tags.key(Registries.POINT_OF_INTEREST_TYPE, "village");
    public static final TagKey<PoiType> BEE_HOME = org.basaltmc.bukkit.facade.Tags.key(Registries.POINT_OF_INTEREST_TYPE, "bee_home");

    private PoiTypeTags() {}
}
