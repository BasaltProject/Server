// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.data.worldgen;
import net.minecraft.resources.ResourceKey;
import net.minecraft.world.level.levelgen.structure.pools.StructureTemplatePool;
public class AbandonedCampStructurePools {
    public static final AbandonedCampStructure BAMBOO_JUNGLE = new AbandonedCampStructure(Pools.createKey("abandoned_camp/camp/bamboo_jungle"), Pools.createKey("abandoned_camp/tent/bamboo_jungle"), "bamboo_jungle");
    public static final AbandonedCampStructure BIRCH_FOREST = new AbandonedCampStructure(Pools.createKey("abandoned_camp/camp/birch_forest"), Pools.createKey("abandoned_camp/tent/birch_forest"), "birch_forest");
    public static final AbandonedCampStructure CHERRY_GROVE = new AbandonedCampStructure(Pools.createKey("abandoned_camp/camp/cherry_grove"), Pools.createKey("abandoned_camp/tent/cherry_grove"), "cherry_grove");
    public static final AbandonedCampStructure DAPPLED_FOREST = new AbandonedCampStructure(Pools.createKey("abandoned_camp/camp/dappled_forest"), Pools.createKey("abandoned_camp/tent/dappled_forest"), "dappled_forest");
    public static final AbandonedCampStructure FLOWER_FOREST = new AbandonedCampStructure(Pools.createKey("abandoned_camp/camp/flower_forest"), Pools.createKey("abandoned_camp/tent/flower_forest"), "flower_forest");
    public static final AbandonedCampStructure FOREST = new AbandonedCampStructure(Pools.createKey("abandoned_camp/camp/forest"), Pools.createKey("abandoned_camp/tent/forest"), "forest");
    public static final AbandonedCampStructure MEADOW = new AbandonedCampStructure(Pools.createKey("abandoned_camp/camp/meadow"), Pools.createKey("abandoned_camp/tent/meadow"), "meadow");
    public static final AbandonedCampStructure OLD_GROWTH_BIRCH_FOREST = new AbandonedCampStructure(Pools.createKey("abandoned_camp/camp/birch_forest"), Pools.createKey("abandoned_camp/tent/birch_forest"), "birch_forest");
    public static final AbandonedCampStructure OLD_GROWTH_PINE_TAIGA = new AbandonedCampStructure(Pools.createKey("abandoned_camp/camp/old_growth_pine_taiga"), Pools.createKey("abandoned_camp/tent/old_growth_pine_taiga"), "old_growth_pine_taiga");
    public static final AbandonedCampStructure OLD_GROWTH_SPRUCE_TAIGA = new AbandonedCampStructure(Pools.createKey("abandoned_camp/camp/old_growth_spruce_taiga"), Pools.createKey("abandoned_camp/tent/old_growth_spruce_taiga"), "old_growth_spruce_taiga");
    public static final AbandonedCampStructure PALE_GARDEN = new AbandonedCampStructure(Pools.createKey("abandoned_camp/camp/pale_garden"), Pools.createKey("abandoned_camp/tent/pale_garden"), "pale_garden");
    public static final AbandonedCampStructure SAVANNA = new AbandonedCampStructure(Pools.createKey("abandoned_camp/camp/savanna"), Pools.createKey("abandoned_camp/tent/savanna"), "savanna");
    public static final AbandonedCampStructure SNOWY_TAIGA = new AbandonedCampStructure(Pools.createKey("abandoned_camp/camp/snowy_taiga"), Pools.createKey("abandoned_camp/tent/snowy_taiga"), "snowy_taiga");
    public static final AbandonedCampStructure SPARSE_JUNGLE = new AbandonedCampStructure(Pools.createKey("abandoned_camp/camp/sparse_jungle"), Pools.createKey("abandoned_camp/tent/sparse_jungle"), "sparse_jungle");
    public static final AbandonedCampStructure SWAMP = new AbandonedCampStructure(Pools.createKey("abandoned_camp/camp/swamp"), Pools.createKey("abandoned_camp/tent/swamp"), "swamp");
    public static final AbandonedCampStructure TAIGA = new AbandonedCampStructure(Pools.createKey("abandoned_camp/camp/taiga"), Pools.createKey("abandoned_camp/tent/taiga"), "taiga");
    public static final AbandonedCampStructure WINDSWEPT_FOREST = new AbandonedCampStructure(Pools.createKey("abandoned_camp/camp/forest"), Pools.createKey("abandoned_camp/tent/forest"), "forest");
    public static final AbandonedCampStructure WOODED_BADLANDS = new AbandonedCampStructure(Pools.createKey("abandoned_camp/camp/wooded_badlands"), Pools.createKey("abandoned_camp/tent/wooded_badlands"), "wooded_badlands");
    public static void bootstrap(BootstrapContext<StructureTemplatePool> context) {
    }
    public static record AbandonedCampStructure(ResourceKey<StructureTemplatePool> campStructureDirectory, ResourceKey<StructureTemplatePool> tentStructureDirectory, String name) {
    }
}
