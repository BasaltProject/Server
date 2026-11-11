// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.data.worldgen;
import net.minecraft.resources.ResourceKey;
import net.minecraft.world.level.levelgen.structure.templatesystem.StructureProcessorList;
public class ProcessorLists {
    public static final ResourceKey<StructureProcessorList> ZOMBIE_PLAINS = k("zombie_plains");
    public static final ResourceKey<StructureProcessorList> ZOMBIE_SAVANNA = k("zombie_savanna");
    public static final ResourceKey<StructureProcessorList> ZOMBIE_SNOWY = k("zombie_snowy");
    public static final ResourceKey<StructureProcessorList> ZOMBIE_TAIGA = k("zombie_taiga");
    public static final ResourceKey<StructureProcessorList> ZOMBIE_DESERT = k("zombie_desert");
    public static final ResourceKey<StructureProcessorList> MOSSIFY_10_PERCENT = k("mossify_10_percent");
    public static final ResourceKey<StructureProcessorList> MOSSIFY_20_PERCENT = k("mossify_20_percent");
    public static final ResourceKey<StructureProcessorList> MOSSIFY_70_PERCENT = k("mossify_70_percent");
    public static final ResourceKey<StructureProcessorList> STREET_PLAINS = k("street_plains");
    public static final ResourceKey<StructureProcessorList> STREET_SAVANNA = k("street_savanna");
    public static final ResourceKey<StructureProcessorList> STREET_SNOWY_OR_TAIGA = k("street_snowy_or_taiga");
    public static final ResourceKey<StructureProcessorList> FARM_PLAINS = k("farm_plains");
    public static final ResourceKey<StructureProcessorList> FARM_SAVANNA = k("farm_savanna");
    public static final ResourceKey<StructureProcessorList> FARM_SNOWY = k("farm_snowy");
    public static final ResourceKey<StructureProcessorList> FARM_TAIGA = k("farm_taiga");
    public static final ResourceKey<StructureProcessorList> FARM_DESERT = k("farm_desert");
    public static final ResourceKey<StructureProcessorList> OUTPOST_ROT = k("outpost_rot");
    public static final ResourceKey<StructureProcessorList> BOTTOM_RAMPART = k("bottom_rampart");
    public static final ResourceKey<StructureProcessorList> TREASURE_ROOMS = k("treasure_rooms");
    public static final ResourceKey<StructureProcessorList> HOUSING = k("housing");
    public static final ResourceKey<StructureProcessorList> SIDE_WALL_DEGRADATION = k("side_wall_degradation");
    public static final ResourceKey<StructureProcessorList> STABLE_DEGRADATION = k("stable_degradation");
    public static final ResourceKey<StructureProcessorList> BASTION_GENERIC_DEGRADATION = k("bastion_generic_degradation");
    public static final ResourceKey<StructureProcessorList> RAMPART_DEGRADATION = k("rampart_degradation");
    public static final ResourceKey<StructureProcessorList> ENTRANCE_REPLACEMENT = k("entrance_replacement");
    public static final ResourceKey<StructureProcessorList> BRIDGE = k("bridge");
    public static final ResourceKey<StructureProcessorList> ROOF = k("roof");
    public static final ResourceKey<StructureProcessorList> HIGH_WALL = k("high_wall");
    public static final ResourceKey<StructureProcessorList> HIGH_RAMPART = k("high_rampart");
    public static final ResourceKey<StructureProcessorList> FOSSIL_ROT = k("fossil_rot");
    public static final ResourceKey<StructureProcessorList> FOSSIL_COAL = k("fossil_coal");
    public static final ResourceKey<StructureProcessorList> FOSSIL_DIAMONDS = k("fossil_diamonds");
    public static final ResourceKey<StructureProcessorList> ANCIENT_CITY_START_DEGRADATION = k("ancient_city_start_degradation");
    public static final ResourceKey<StructureProcessorList> ANCIENT_CITY_GENERIC_DEGRADATION = k("ancient_city_generic_degradation");
    public static final ResourceKey<StructureProcessorList> ANCIENT_CITY_WALLS_DEGRADATION = k("ancient_city_walls_degradation");
    public static final ResourceKey<StructureProcessorList> TRAIL_RUINS_HOUSES_ARCHAEOLOGY = k("trail_ruins_houses_archaeology");
    public static final ResourceKey<StructureProcessorList> TRAIL_RUINS_ROADS_ARCHAEOLOGY = k("trail_ruins_roads_archaeology");
    public static final ResourceKey<StructureProcessorList> TRAIL_RUINS_TOWER_TOP_ARCHAEOLOGY = k("trail_ruins_tower_top_archaeology");
    public static final ResourceKey<StructureProcessorList> TRIAL_CHAMBERS_COPPER_BULB_DEGRADATION = k("trial_chambers_copper_bulb_degradation");
    public static void bootstrap(BootstrapContext<StructureProcessorList> context) {
    }

    @SuppressWarnings({"unchecked", "rawtypes"})
    private static ResourceKey<StructureProcessorList> k(String n) {
        return (ResourceKey) net.minecraft.resources.ResourceKey.create(net.minecraft.core.registries.Registries.PROCESSOR_LIST, net.minecraft.resources.Identifier.withDefaultNamespace(n));
    }
}
