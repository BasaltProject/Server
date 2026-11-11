// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.levelgen.structure;
import com.mojang.serialization.MapCodec;
import net.minecraft.world.level.levelgen.structure.structures.BuriedTreasureStructure;
import net.minecraft.world.level.levelgen.structure.structures.DesertPyramidStructure;
import net.minecraft.world.level.levelgen.structure.structures.EndCityStructure;
import net.minecraft.world.level.levelgen.structure.structures.IglooStructure;
import net.minecraft.world.level.levelgen.structure.structures.JigsawStructure;
import net.minecraft.world.level.levelgen.structure.structures.JungleTempleStructure;
import net.minecraft.world.level.levelgen.structure.structures.MineshaftStructure;
import net.minecraft.world.level.levelgen.structure.structures.NetherFortressStructure;
import net.minecraft.world.level.levelgen.structure.structures.NetherFossilStructure;
import net.minecraft.world.level.levelgen.structure.structures.OceanMonumentStructure;
import net.minecraft.world.level.levelgen.structure.structures.OceanRuinStructure;
import net.minecraft.world.level.levelgen.structure.structures.RuinedPortalStructure;
import net.minecraft.world.level.levelgen.structure.structures.ShipwreckStructure;
import net.minecraft.world.level.levelgen.structure.structures.StrongholdStructure;
import net.minecraft.world.level.levelgen.structure.structures.SwampHutStructure;
import net.minecraft.world.level.levelgen.structure.structures.WoodlandMansionStructure;
public interface StructureType<S extends Structure> {
    StructureType<BuriedTreasureStructure> BURIED_TREASURE = listed("buried_treasure", BuriedTreasureStructure.CODEC);
    StructureType<DesertPyramidStructure> DESERT_PYRAMID = listed("desert_pyramid", DesertPyramidStructure.CODEC);
    StructureType<EndCityStructure> END_CITY = listed("end_city", EndCityStructure.CODEC);
    StructureType<NetherFortressStructure> FORTRESS = listed("fortress", NetherFortressStructure.CODEC);
    StructureType<IglooStructure> IGLOO = listed("igloo", IglooStructure.CODEC);
    StructureType<JigsawStructure> JIGSAW = listed("jigsaw", JigsawStructure.CODEC);
    StructureType<JungleTempleStructure> JUNGLE_TEMPLE = listed("jungle_temple", JungleTempleStructure.CODEC);
    StructureType<MineshaftStructure> MINESHAFT = listed("mineshaft", MineshaftStructure.CODEC);
    StructureType<NetherFossilStructure> NETHER_FOSSIL = listed("nether_fossil", NetherFossilStructure.CODEC);
    StructureType<OceanMonumentStructure> OCEAN_MONUMENT = listed("ocean_monument", OceanMonumentStructure.CODEC);
    StructureType<OceanRuinStructure> OCEAN_RUIN = listed("ocean_ruin", OceanRuinStructure.CODEC);
    StructureType<RuinedPortalStructure> RUINED_PORTAL = listed("ruined_portal", RuinedPortalStructure.CODEC);
    StructureType<ShipwreckStructure> SHIPWRECK = listed("shipwreck", ShipwreckStructure.CODEC);
    StructureType<StrongholdStructure> STRONGHOLD = listed("stronghold", StrongholdStructure.CODEC);
    StructureType<SwampHutStructure> SWAMP_HUT = listed("swamp_hut", SwampHutStructure.CODEC);
    StructureType<WoodlandMansionStructure> WOODLAND_MANSION = listed("woodland_mansion", WoodlandMansionStructure.CODEC);
    MapCodec<S> codec();
    private static <S extends Structure> StructureType<S> listed(String id, MapCodec<S> codec) {
        return net.minecraft.core.Registry.register(net.minecraft.core.registries.BuiltInRegistries.STRUCTURE_TYPE, id, () -> codec);
    }
}
