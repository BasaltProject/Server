// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.levelgen.structure.pieces;
import net.minecraft.world.level.levelgen.structure.structures.WoodlandMansionPieces;
import net.minecraft.world.level.levelgen.structure.structures.SwampHutPiece;
import net.minecraft.world.level.levelgen.structure.structures.StrongholdPieces;
import net.minecraft.world.level.levelgen.structure.structures.ShipwreckPieces;
import net.minecraft.world.level.levelgen.structure.structures.RuinedPortalPiece;
import net.minecraft.world.level.levelgen.structure.structures.OceanRuinPieces;
import net.minecraft.world.level.levelgen.structure.structures.OceanMonumentPieces;
import net.minecraft.world.level.levelgen.structure.structures.NetherFossilPieces;
import net.minecraft.world.level.levelgen.structure.structures.NetherFortressPieces;
import net.minecraft.world.level.levelgen.structure.structures.MineshaftPieces;
import net.minecraft.world.level.levelgen.structure.structures.JungleTemplePiece;
import net.minecraft.world.level.levelgen.structure.structures.IglooPieces;
import net.minecraft.world.level.levelgen.structure.structures.EndCityPieces;
import net.minecraft.world.level.levelgen.structure.structures.DesertPyramidPiece;
import net.minecraft.world.level.levelgen.structure.structures.BuriedTreasurePieces;
import net.minecraft.world.level.levelgen.structure.PoolElementStructurePiece;
import net.minecraft.nbt.CompoundTag;
import net.minecraft.world.level.levelgen.structure.StructurePiece;
import net.minecraft.world.level.levelgen.structure.templatesystem.StructureTemplateManager;
public interface StructurePieceType {
    private static StructurePieceType fullCtx(StructurePieceType type, String id) {
        return net.minecraft.core.Registry.register(net.minecraft.core.registries.BuiltInRegistries.STRUCTURE_PIECE, id.toLowerCase(java.util.Locale.ROOT), type);
    }
    private static StructurePieceType ctxless(ContextlessType type, String id) {
        return fullCtx(type, id);
    }
    private static StructurePieceType templated(StructureTemplateType type, String id) {
        return fullCtx(type, id);
    }
    StructurePieceType MINE_SHAFT_CORRIDOR = ctxless(MineshaftPieces.MineShaftCorridor::new, "MSCorridor");
    StructurePieceType MINE_SHAFT_CROSSING = ctxless(MineshaftPieces.MineShaftCrossing::new, "MSCrossing");
    StructurePieceType MINE_SHAFT_ROOM = ctxless(MineshaftPieces.MineShaftRoom::new, "MSRoom");
    StructurePieceType MINE_SHAFT_STAIRS = ctxless(MineshaftPieces.MineShaftStairs::new, "MSStairs");
    StructurePieceType NETHER_FORTRESS_BRIDGE_CROSSING = ctxless(NetherFortressPieces.BridgeCrossing::new, "NeBCr");
    StructurePieceType NETHER_FORTRESS_BRIDGE_END_FILLER = ctxless(NetherFortressPieces.BridgeEndFiller::new, "NeBEF");
    StructurePieceType NETHER_FORTRESS_BRIDGE_STRAIGHT = ctxless(NetherFortressPieces.BridgeStraight::new, "NeBS");
    StructurePieceType NETHER_FORTRESS_CASTLE_CORRIDOR_STAIRS = ctxless(NetherFortressPieces.CastleCorridorStairsPiece::new, "NeCCS");
    StructurePieceType NETHER_FORTRESS_CASTLE_CORRIDOR_T_BALCONY = ctxless(NetherFortressPieces.CastleCorridorTBalconyPiece::new, "NeCTB");
    StructurePieceType NETHER_FORTRESS_CASTLE_ENTRANCE = ctxless(NetherFortressPieces.CastleEntrance::new, "NeCE");
    StructurePieceType NETHER_FORTRESS_CASTLE_SMALL_CORRIDOR_CROSSING = ctxless(NetherFortressPieces.CastleSmallCorridorCrossingPiece::new, "NeSCSC");
    StructurePieceType NETHER_FORTRESS_CASTLE_SMALL_CORRIDOR_LEFT_TURN = ctxless(NetherFortressPieces.CastleSmallCorridorLeftTurnPiece::new, "NeSCLT");
    StructurePieceType NETHER_FORTRESS_CASTLE_SMALL_CORRIDOR = ctxless(NetherFortressPieces.CastleSmallCorridorPiece::new, "NeSC");
    StructurePieceType NETHER_FORTRESS_CASTLE_SMALL_CORRIDOR_RIGHT_TURN = ctxless(NetherFortressPieces.CastleSmallCorridorRightTurnPiece::new, "NeSCRT");
    StructurePieceType NETHER_FORTRESS_CASTLE_STALK_ROOM = ctxless(NetherFortressPieces.CastleStalkRoom::new, "NeCSR");
    StructurePieceType NETHER_FORTRESS_MONSTER_THRONE = ctxless(NetherFortressPieces.MonsterThrone::new, "NeMT");
    StructurePieceType NETHER_FORTRESS_ROOM_CROSSING = ctxless(NetherFortressPieces.RoomCrossing::new, "NeRC");
    StructurePieceType NETHER_FORTRESS_STAIRS_ROOM = ctxless(NetherFortressPieces.StairsRoom::new, "NeSR");
    StructurePieceType NETHER_FORTRESS_START = ctxless(NetherFortressPieces.StartPiece::new, "NeStart");
    StructurePieceType STRONGHOLD_CHEST_CORRIDOR = ctxless(StrongholdPieces.ChestCorridor::new, "SHCC");
    StructurePieceType STRONGHOLD_FILLER_CORRIDOR = ctxless(StrongholdPieces.FillerCorridor::new, "SHFC");
    StructurePieceType STRONGHOLD_FIVE_CROSSING = ctxless(StrongholdPieces.FiveCrossing::new, "SH5C");
    StructurePieceType STRONGHOLD_LEFT_TURN = ctxless(StrongholdPieces.LeftTurn::new, "SHLT");
    StructurePieceType STRONGHOLD_LIBRARY = ctxless(StrongholdPieces.Library::new, "SHLi");
    StructurePieceType STRONGHOLD_PORTAL_ROOM = ctxless(StrongholdPieces.PortalRoom::new, "SHPR");
    StructurePieceType STRONGHOLD_PRISON_HALL = ctxless(StrongholdPieces.PrisonHall::new, "SHPH");
    StructurePieceType STRONGHOLD_RIGHT_TURN = ctxless(StrongholdPieces.RightTurn::new, "SHRT");
    StructurePieceType STRONGHOLD_ROOM_CROSSING = ctxless(StrongholdPieces.RoomCrossing::new, "SHRC");
    StructurePieceType STRONGHOLD_STAIRS_DOWN = ctxless(StrongholdPieces.StairsDown::new, "SHSD");
    StructurePieceType STRONGHOLD_START = ctxless(StrongholdPieces.StartPiece::new, "SHStart");
    StructurePieceType STRONGHOLD_STRAIGHT = ctxless(StrongholdPieces.Straight::new, "SHS");
    StructurePieceType STRONGHOLD_STRAIGHT_STAIRS_DOWN = ctxless(StrongholdPieces.StraightStairsDown::new, "SHSSD");
    StructurePieceType JUNGLE_PYRAMID_PIECE = ctxless(JungleTemplePiece::new, "TeJP");
    StructurePieceType OCEAN_RUIN = templated(OceanRuinPieces.OceanRuinPiece::create, "ORP");
    StructurePieceType IGLOO = templated(IglooPieces.IglooPiece::new, "Iglu");
    StructurePieceType RUINED_PORTAL = fullCtx(RuinedPortalPiece::new, "RUPO");
    StructurePieceType SWAMPLAND_HUT = ctxless(SwampHutPiece::new, "TeSH");
    StructurePieceType DESERT_PYRAMID_PIECE = ctxless(DesertPyramidPiece::new, "TeDP");
    StructurePieceType OCEAN_MONUMENT_BUILDING = ctxless(OceanMonumentPieces.MonumentBuilding::new, "OMB");
    StructurePieceType OCEAN_MONUMENT_CORE_ROOM = ctxless(OceanMonumentPieces.OceanMonumentCoreRoom::new, "OMCR");
    StructurePieceType OCEAN_MONUMENT_DOUBLE_X_ROOM = ctxless(OceanMonumentPieces.OceanMonumentDoubleXRoom::new, "OMDXR");
    StructurePieceType OCEAN_MONUMENT_DOUBLE_XY_ROOM = ctxless(OceanMonumentPieces.OceanMonumentDoubleXYRoom::new, "OMDXYR");
    StructurePieceType OCEAN_MONUMENT_DOUBLE_Y_ROOM = ctxless(OceanMonumentPieces.OceanMonumentDoubleYRoom::new, "OMDYR");
    StructurePieceType OCEAN_MONUMENT_DOUBLE_YZ_ROOM = ctxless(OceanMonumentPieces.OceanMonumentDoubleYZRoom::new, "OMDYZR");
    StructurePieceType OCEAN_MONUMENT_DOUBLE_Z_ROOM = ctxless(OceanMonumentPieces.OceanMonumentDoubleZRoom::new, "OMDZR");
    StructurePieceType OCEAN_MONUMENT_ENTRY_ROOM = ctxless(OceanMonumentPieces.OceanMonumentEntryRoom::new, "OMEntry");
    StructurePieceType OCEAN_MONUMENT_PENTHOUSE = ctxless(OceanMonumentPieces.OceanMonumentPenthouse::new, "OMPenthouse");
    StructurePieceType OCEAN_MONUMENT_SIMPLE_ROOM = ctxless(OceanMonumentPieces.OceanMonumentSimpleRoom::new, "OMSimple");
    StructurePieceType OCEAN_MONUMENT_SIMPLE_TOP_ROOM = ctxless(OceanMonumentPieces.OceanMonumentSimpleTopRoom::new, "OMSimpleT");
    StructurePieceType OCEAN_MONUMENT_WING_ROOM = ctxless(OceanMonumentPieces.OceanMonumentWingRoom::new, "OMWR");
    StructurePieceType END_CITY_PIECE = templated(EndCityPieces.EndCityPiece::new, "ECP");
    StructurePieceType WOODLAND_MANSION_PIECE = templated(WoodlandMansionPieces.WoodlandMansionPiece::new, "WMP");
    StructurePieceType BURIED_TREASURE_PIECE = ctxless(BuriedTreasurePieces.BuriedTreasurePiece::new, "BTP");
    StructurePieceType SHIPWRECK_PIECE = templated(ShipwreckPieces.ShipwreckPiece::new, "Shipwreck");
    StructurePieceType NETHER_FOSSIL = templated(NetherFossilPieces.NetherFossilPiece::new, "NeFos");
    StructurePieceType JIGSAW = fullCtx(PoolElementStructurePiece::new, "jigsaw");
    StructurePiece load(StructurePieceSerializationContext context, CompoundTag tag);
    public interface ContextlessType extends StructurePieceType {
        StructurePiece load(CompoundTag tag);
        default StructurePiece load(StructurePieceSerializationContext context, CompoundTag tag) {
            return load(tag);
        }
    }
    public interface StructureTemplateType extends StructurePieceType {
        StructurePiece load(StructureTemplateManager structureTemplateManager, CompoundTag tag);
        default StructurePiece load(StructurePieceSerializationContext context, CompoundTag tag) {
            return load(context.structureTemplateManager(), tag);


        }
    }
}
