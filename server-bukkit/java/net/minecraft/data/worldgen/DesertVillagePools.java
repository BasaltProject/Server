// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.data.worldgen;
import net.minecraft.resources.ResourceKey;
import net.minecraft.world.level.levelgen.structure.pools.StructureTemplatePool;
public class DesertVillagePools {
    public static final ResourceKey<StructureTemplatePool> START = k("village/desert/town_centers");
    public static void bootstrap(BootstrapContext<StructureTemplatePool> context) {
    }

    private static ResourceKey<StructureTemplatePool> k(String n) {
        return Pools.createKey(n);
    }
}
