// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.levelgen.structure;
import net.minecraft.resources.ResourceKey;
public interface BuiltinStructures {
    @SuppressWarnings({"unchecked", "rawtypes"})
    private static <T> ResourceKey<T> key(String name) {
        return (ResourceKey) ResourceKey.create(net.minecraft.core.registries.Registries.STRUCTURE,
            net.minecraft.resources.Identifier.withDefaultNamespace(name));
    }

    ResourceKey<Structure> PILLAGER_OUTPOST = key("pillager_outpost");
    ResourceKey<Structure> MINESHAFT = key("mineshaft");
    ResourceKey<Structure> MINESHAFT_MESA = key("mineshaft_mesa");
    ResourceKey<Structure> WOODLAND_MANSION = key("woodland_mansion");
    ResourceKey<Structure> JUNGLE_TEMPLE = key("jungle_temple");
    ResourceKey<Structure> DESERT_PYRAMID = key("desert_pyramid");
    ResourceKey<Structure> IGLOO = key("igloo");
    ResourceKey<Structure> SHIPWRECK = key("shipwreck");
    ResourceKey<Structure> SHIPWRECK_BEACHED = key("shipwreck_beached");
    ResourceKey<Structure> SWAMP_HUT = key("swamp_hut");
    ResourceKey<Structure> STRONGHOLD = key("stronghold");
    ResourceKey<Structure> OCEAN_MONUMENT = key("ocean_monument");
    ResourceKey<Structure> OCEAN_RUIN_COLD = key("ocean_ruin_cold");
    ResourceKey<Structure> OCEAN_RUIN_WARM = key("ocean_ruin_warm");
    ResourceKey<Structure> FORTRESS = key("fortress");
    ResourceKey<Structure> NETHER_FOSSIL = key("nether_fossil");
    ResourceKey<Structure> END_CITY = key("end_city");
    ResourceKey<Structure> BURIED_TREASURE = key("buried_treasure");
    ResourceKey<Structure> BASTION_REMNANT = key("bastion_remnant");
    ResourceKey<Structure> VILLAGE_PLAINS = key("village_plains");
    ResourceKey<Structure> VILLAGE_DESERT = key("village_desert");
    ResourceKey<Structure> VILLAGE_SAVANNA = key("village_savanna");
    ResourceKey<Structure> VILLAGE_SNOWY = key("village_snowy");
    ResourceKey<Structure> VILLAGE_TAIGA = key("village_taiga");
    ResourceKey<Structure> RUINED_PORTAL_STANDARD = key("ruined_portal_standard");
    ResourceKey<Structure> RUINED_PORTAL_DESERT = key("ruined_portal_desert");
    ResourceKey<Structure> RUINED_PORTAL_JUNGLE = key("ruined_portal_jungle");
    ResourceKey<Structure> RUINED_PORTAL_SWAMP = key("ruined_portal_swamp");
    ResourceKey<Structure> RUINED_PORTAL_MOUNTAIN = key("ruined_portal_mountain");
    ResourceKey<Structure> RUINED_PORTAL_OCEAN = key("ruined_portal_ocean");
    ResourceKey<Structure> RUINED_PORTAL_NETHER = key("ruined_portal_nether");
    ResourceKey<Structure> ANCIENT_CITY = key("ancient_city");
    ResourceKey<Structure> TRAIL_RUINS = key("trail_ruins");
    ResourceKey<Structure> TRIAL_CHAMBERS = key("trial_chambers");
    ResourceKey<Structure> ABANDONED_CAMP_BAMBOO_JUNGLE = key("abandoned_camp_bamboo_jungle");
    ResourceKey<Structure> ABANDONED_CAMP_BIRCH_FOREST = key("abandoned_camp_birch_forest");
    ResourceKey<Structure> ABANDONED_CAMP_CHERRY_GROVE = key("abandoned_camp_cherry_grove");
    ResourceKey<Structure> ABANDONED_CAMP_DAPPLED_FOREST = key("abandoned_camp_dappled_forest");
    ResourceKey<Structure> ABANDONED_CAMP_FLOWER_FOREST = key("abandoned_camp_flower_forest");
    ResourceKey<Structure> ABANDONED_CAMP_FOREST = key("abandoned_camp_forest");
    ResourceKey<Structure> ABANDONED_CAMP_MEADOW = key("abandoned_camp_meadow");
    ResourceKey<Structure> ABANDONED_CAMP_OLD_GROWTH_BIRCH_FOREST = key("abandoned_camp_old_growth_birch_forest");
    ResourceKey<Structure> ABANDONED_CAMP_OLD_GROWTH_PINE_TAIGA = key("abandoned_camp_old_growth_pine_taiga");
    ResourceKey<Structure> ABANDONED_CAMP_OLD_GROWTH_SPRUCE_TAIGA = key("abandoned_camp_old_growth_spruce_taiga");
    ResourceKey<Structure> ABANDONED_CAMP_PALE_GARDEN = key("abandoned_camp_pale_garden");
    ResourceKey<Structure> ABANDONED_CAMP_SAVANNA = key("abandoned_camp_savanna");
    ResourceKey<Structure> ABANDONED_CAMP_SNOWY_TAIGA = key("abandoned_camp_snowy_taiga");
    ResourceKey<Structure> ABANDONED_CAMP_SPARSE_JUNGLE = key("abandoned_camp_sparse_jungle");
    ResourceKey<Structure> ABANDONED_CAMP_SWAMP = key("abandoned_camp_swamp");
    ResourceKey<Structure> ABANDONED_CAMP_TAIGA = key("abandoned_camp_taiga");
    ResourceKey<Structure> ABANDONED_CAMP_WINDSWEPT_FOREST = key("abandoned_camp_windswept_forest");
    ResourceKey<Structure> ABANDONED_CAMP_WOODED_BADLANDS = key("abandoned_camp_wooded_badlands");
}
