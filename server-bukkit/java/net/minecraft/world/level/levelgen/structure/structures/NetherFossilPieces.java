// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.levelgen.structure.structures;
import net.minecraft.world.level.levelgen.structure.pieces.StructurePieceType;
import net.minecraft.world.level.levelgen.structure.templatesystem.BlockIgnoreProcessor;
import net.minecraft.util.Util;
import net.minecraft.world.level.block.Blocks;
import net.minecraft.world.level.block.Mirror;
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
public class NetherFossilPieces {
    private static final Identifier[] kFossils = {Identifier.withDefaultNamespace("nether_fossils/fossil_1"), Identifier.withDefaultNamespace("nether_fossils/fossil_2"),
        Identifier.withDefaultNamespace("nether_fossils/fossil_3"), Identifier.withDefaultNamespace("nether_fossils/fossil_4"), Identifier.withDefaultNamespace("nether_fossils/fossil_5"),
        Identifier.withDefaultNamespace("nether_fossils/fossil_6"), Identifier.withDefaultNamespace("nether_fossils/fossil_7"), Identifier.withDefaultNamespace("nether_fossils/fossil_8"),
        Identifier.withDefaultNamespace("nether_fossils/fossil_9"), Identifier.withDefaultNamespace("nether_fossils/fossil_10"), Identifier.withDefaultNamespace("nether_fossils/fossil_11"),
        Identifier.withDefaultNamespace("nether_fossils/fossil_12"), Identifier.withDefaultNamespace("nether_fossils/fossil_13"), Identifier.withDefaultNamespace("nether_fossils/fossil_14")};
    public static void addPieces(StructureTemplateManager structureTemplateManager, StructurePiecesBuilder builder, RandomSource random, BlockPos position) {
        Rotation turn = Rotation.getRandom(random);
        builder.addPiece(new NetherFossilPiece(structureTemplateManager, kFossils[random.nextInt(kFossils.length)], position, turn));
    }
    public static class NetherFossilPiece extends TemplateStructurePiece {
        private static StructurePlaceSettings settingsFor(Rotation rotation) {
            return new StructurePlaceSettings().setRotation(rotation).setMirror(Mirror.NONE).addProcessor(BlockIgnoreProcessor.STRUCTURE_AND_AIR);
        }
        private void dropGhast(WorldGenLevel level, BoundingBox fossilBox, BoundingBox chunkBB) {
            RandomSource local = RandomSource.createThreadLocalInstance(level.getSeed()).forkPositional().at(fossilBox.getCenter());
            if (local.nextFloat() >= 0.5F) {
                return;
            }
            BlockPos spot = new BlockPos(fossilBox.minX() + local.nextInt(fossilBox.getXSpan()), fossilBox.minY(), fossilBox.minZ() + local.nextInt(fossilBox.getZSpan()));
            if (level.getBlockState(spot).isAir() && chunkBB.isInside(spot)) {
                level.setBlock(spot, Blocks.DRIED_GHAST.defaultBlockState().rotate(Rotation.getRandom(local)), 2);
            }
        }

        public NetherFossilPiece(StructureTemplateManager structureTemplateManager, Identifier templateLocation, BlockPos position, Rotation rotation) {
            super(StructurePieceType.NETHER_FOSSIL, 0, structureTemplateManager, templateLocation, templateLocation.toString(), settingsFor(rotation), position);
        }
        public NetherFossilPiece(StructureTemplateManager structureTemplateManager, CompoundTag tag) {
            super(StructurePieceType.NETHER_FOSSIL, tag, structureTemplateManager, location -> settingsFor(Rotation.valueOf(tag.getStringOr("Rot", "NONE"))));
        }
        protected void addAdditionalSaveData(StructurePieceSerializationContext context, CompoundTag tag) {
            super.addAdditionalSaveData(context, tag);
            tag.putString("Rot", placeSettings.getRotation().name());
        }
        protected void handleDataMarker(String markerId, BlockPos position, ServerLevelAccessor level, RandomSource random, BoundingBox chunkBB) {
            return;
        }
        public void postProcess(WorldGenLevel level, StructureManager structureManager, ChunkGenerator generator, RandomSource random, BoundingBox chunkBB, ChunkPos chunkPos, BlockPos referencePos) {
            BoundingBox fossilBox = template.getBoundingBox(placeSettings, templatePosition);
            chunkBB.encapsulate(fossilBox);
            super.postProcess(level, structureManager, generator, random, chunkBB, chunkPos, referencePos);
            dropGhast(level, fossilBox, chunkBB);
        }
    }
}
