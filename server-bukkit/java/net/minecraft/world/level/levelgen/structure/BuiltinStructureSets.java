// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.levelgen.structure;
import net.minecraft.resources.ResourceKey;
public interface BuiltinStructureSets {
    @SuppressWarnings({"unchecked", "rawtypes"})
    private static <T> ResourceKey<T> key(String name) {
        return (ResourceKey) ResourceKey.create(net.minecraft.core.registries.Registries.STRUCTURE_SET,
            net.minecraft.resources.Identifier.withDefaultNamespace(name));
    }

    ResourceKey<StructureSet> VILLAGES = key("villages");
    ResourceKey<StructureSet> DESERT_PYRAMIDS = key("desert_pyramids");
    ResourceKey<StructureSet> IGLOOS = key("igloos");
    ResourceKey<StructureSet> JUNGLE_TEMPLES = key("jungle_temples");
    ResourceKey<StructureSet> SWAMP_HUTS = key("swamp_huts");
    ResourceKey<StructureSet> PILLAGER_OUTPOSTS = key("pillager_outposts");
    ResourceKey<StructureSet> OCEAN_MONUMENTS = key("ocean_monuments");
    ResourceKey<StructureSet> WOODLAND_MANSIONS = key("woodland_mansions");
    ResourceKey<StructureSet> BURIED_TREASURES = key("buried_treasures");
    ResourceKey<StructureSet> MINESHAFTS = key("mineshafts");
    ResourceKey<StructureSet> RUINED_PORTALS = key("ruined_portals");
    ResourceKey<StructureSet> SHIPWRECKS = key("shipwrecks");
    ResourceKey<StructureSet> OCEAN_RUINS = key("ocean_ruins");
    ResourceKey<StructureSet> NETHER_COMPLEXES = key("nether_complexes");
    ResourceKey<StructureSet> NETHER_FOSSILS = key("nether_fossils");
    ResourceKey<StructureSet> END_CITIES = key("end_cities");
    ResourceKey<StructureSet> ANCIENT_CITIES = key("ancient_cities");
    ResourceKey<StructureSet> STRONGHOLDS = key("strongholds");
    ResourceKey<StructureSet> TRAIL_RUINS = key("trail_ruins");
    ResourceKey<StructureSet> TRIAL_CHAMBERS = key("trial_chambers");
    ResourceKey<StructureSet> ABANDONED_CAMP = key("abandoned_camp");
}
