// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.levelgen.structure.structures;
import net.minecraft.world.level.levelgen.structure.pieces.StructurePieceType;
import net.minecraft.world.level.levelgen.structure.templatesystem.BlockIgnoreProcessor;
import net.minecraft.world.level.levelgen.structure.templatesystem.LiquidSettings;
import net.minecraft.world.level.levelgen.Heightmap;
import net.minecraft.world.level.storage.loot.BuiltInLootTables;
import net.minecraft.world.RandomizableContainer;
import net.minecraft.core.Vec3i;
import java.util.Map;
import net.minecraft.world.level.block.Blocks;
import net.minecraft.world.level.block.state.BlockState;
import net.minecraft.world.level.block.Mirror;
import net.minecraft.world.level.levelgen.structure.templatesystem.StructureTemplate;
import net.minecraft.world.level.levelgen.structure.templatesystem.StructurePlaceSettings;
import net.minecraft.core.BlockPos;
import net.minecraft.nbt.CompoundTag;
import net.minecraft.resources.Identifier;
import net.minecraft.util.RandomSource;
import net.minecraft.world.level.ChunkPos;
import net.minecraft.world.level.ServerLevelAccessor;
import net.minecraft.world.level.StructureManager;
import net.minecraft.world.level.WorldGenLevel;
import net.minecraft.world.level.block.Rotation;
import net.minecraft.world.level.chunk.ChunkGenerator;
import net.minecraft.world.level.levelgen.structure.BoundingBox;
import net.minecraft.world.level.levelgen.structure.TemplateStructurePiece;
import net.minecraft.world.level.levelgen.structure.pieces.StructurePieceSerializationContext;
import net.minecraft.world.level.levelgen.structure.pieces.StructurePiecesBuilder;
import net.minecraft.world.level.levelgen.structure.templatesystem.StructureTemplateManager;
public class IglooPieces {
    private static final Identifier kTop = Identifier.withDefaultNamespace("igloo/top");
    private static final Identifier kMiddle = Identifier.withDefaultNamespace("igloo/middle");
    private static final Identifier kBottom = Identifier.withDefaultNamespace("igloo/bottom");
    private static final Map<Identifier, BlockPos> kPivots = Map.of(kTop, new BlockPos(3, 5, 5), kMiddle, new BlockPos(1, 3, 1), kBottom, new BlockPos(3, 6, 7));
    private static final Map<Identifier, BlockPos> kOffsets = Map.of(kTop, BlockPos.ZERO, kMiddle, new BlockPos(2, -3, 4), kBottom, new BlockPos(0, -3, -2));
    public static final int GENERATION_HEIGHT = 90;
    public static void addPieces(StructureTemplateManager structureTemplateManager, BlockPos position, Rotation rotation, StructurePiecesBuilder builder, RandomSource random) {
        if (random.nextDouble() < 0.5D) {
            int depth = random.nextInt(8) + 4;
            builder.addPiece(new IglooPiece(structureTemplateManager, kBottom, position, rotation, depth * 3));
            for (int i = 0; i < depth - 1; i++) {
                builder.addPiece(new IglooPiece(structureTemplateManager, kMiddle, position, rotation, i * 3));
            }
        }
        builder.addPiece(new IglooPiece(structureTemplateManager, kTop, position, rotation, 0));
    }
    public static class IglooPiece extends TemplateStructurePiece {
        private static StructurePlaceSettings settingsFor(Rotation rotation, Identifier id) {
            return new StructurePlaceSettings().setRotation(rotation).setMirror(Mirror.NONE).setRotationPivot(kPivots.get(id)).addProcessor(BlockIgnoreProcessor.STRUCTURE_BLOCK).setLiquidSettings(LiquidSettings.IGNORE_WATERLOGGING);
        }

        public IglooPiece(StructureTemplateManager structureTemplateManager, Identifier templateLocation, BlockPos position, Rotation rotation, int depth) {
            super(StructurePieceType.IGLOO, 0, structureTemplateManager, templateLocation, templateLocation.toString(), settingsFor(rotation, templateLocation), position.offset(kOffsets.get(templateLocation)).below(depth));
        }
        public IglooPiece(StructureTemplateManager structureTemplateManager, CompoundTag tag) {
            super(StructurePieceType.IGLOO, tag, structureTemplateManager, location -> settingsFor(Rotation.valueOf(tag.getStringOr("Rot", "NONE")), location));
        }
        protected void addAdditionalSaveData(StructurePieceSerializationContext context, CompoundTag tag) {
            super.addAdditionalSaveData(context, tag);
            tag.putString("Rot", placeSettings.getRotation().name());
        }
        protected void handleDataMarker(String markerId, BlockPos position, ServerLevelAccessor level, RandomSource random, BoundingBox chunkBB) {
            if (!"chest".equals(markerId)) {
                return;
            }
            level.setBlockAndUpdate(position, Blocks.AIR.defaultBlockState());
            if (level.getBlockEntity(position.below()) instanceof RandomizableContainer chest) {
                chest.setLootTable(BuiltInLootTables.IGLOO_CHEST, random.nextLong());
            }
        }
        public void postProcess(WorldGenLevel level, StructureManager structureManager, ChunkGenerator generator, RandomSource random, BoundingBox chunkBB, ChunkPos chunkPos, BlockPos referencePos) {
            Identifier id = Identifier.parse(templateName);
            StructurePlaceSettings settings = settingsFor(placeSettings.getRotation(), id);
            BlockPos offset = kOffsets.get(id);
            BlockPos door = templatePosition.offset(StructureTemplate.calculateRelativePosition(settings, new BlockPos(3 - offset.getX(), 0, -offset.getZ())));
            int surface = level.getHeight(Heightmap.Types.WORLD_SURFACE_WG, door.getX(), door.getZ());
            BlockPos saved = templatePosition;
            templatePosition = templatePosition.offset(0, surface - 90 - 1, 0);
            super.postProcess(level, structureManager, generator, random, chunkBB, chunkPos, referencePos);
            if (id.equals(kTop)) {
                BlockPos hatch = templatePosition.offset(StructureTemplate.calculateRelativePosition(settings, new BlockPos(3, 0, 5)));
                BlockState under = level.getBlockState(hatch.below());
                if (!under.isAir() && !under.is(Blocks.LADDER)) {
                    level.setBlockAndUpdate(hatch, Blocks.SNOW_BLOCK.defaultBlockState());
                }
            }
            templatePosition = saved;
        }
    }
}
