// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.data.worldgen;
import net.minecraft.resources.ResourceKey;
import net.minecraft.world.level.levelgen.structure.pools.StructureTemplatePool;
public class AncientCityStructurePieces {
    public static final ResourceKey<StructureTemplatePool> START = k("ancient_city/city_center");
    public static void bootstrap(BootstrapContext<StructureTemplatePool> context) {
    }

    private static ResourceKey<StructureTemplatePool> k(String n) {
        return Pools.createKey(n);
    }
}
