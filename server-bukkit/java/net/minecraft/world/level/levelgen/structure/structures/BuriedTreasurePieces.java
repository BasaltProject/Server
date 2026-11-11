// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.levelgen.structure.structures;
import net.minecraft.world.level.levelgen.structure.pieces.StructurePieceType;
import net.minecraft.world.level.levelgen.Heightmap;
import net.minecraft.world.level.storage.loot.BuiltInLootTables;
import net.minecraft.world.level.block.Blocks;
import net.minecraft.world.level.block.state.BlockState;
import net.minecraft.core.Direction;
import net.minecraft.core.BlockPos;
import net.minecraft.nbt.CompoundTag;
import net.minecraft.util.RandomSource;
import net.minecraft.world.level.ChunkPos;
import net.minecraft.world.level.StructureManager;
import net.minecraft.world.level.WorldGenLevel;
import net.minecraft.world.level.chunk.ChunkGenerator;
import net.minecraft.world.level.levelgen.structure.BoundingBox;
import net.minecraft.world.level.levelgen.structure.StructurePiece;
import net.minecraft.world.level.levelgen.structure.pieces.StructurePieceSerializationContext;
public class BuriedTreasurePieces {
    public static class BuriedTreasurePiece extends StructurePiece {
        private static boolean wet(BlockState state) {
            return state.is(Blocks.WATER) || state.is(Blocks.LAVA);
        }

        public BuriedTreasurePiece(BlockPos offset) {
            super(StructurePieceType.BURIED_TREASURE_PIECE, 0, new BoundingBox(offset));
        }
        public BuriedTreasurePiece(CompoundTag tag) {
            super(StructurePieceType.BURIED_TREASURE_PIECE, tag);
        }
        protected void addAdditionalSaveData(StructurePieceSerializationContext context, CompoundTag tag) {
            return;
        }
        public void postProcess(WorldGenLevel level, StructureManager structureManager, ChunkGenerator generator, RandomSource random, BoundingBox chunkBB, ChunkPos chunkPos, BlockPos referencePos) {
            int top = level.getHeight(Heightmap.Types.OCEAN_FLOOR_WG, boundingBox.minX(), boundingBox.minZ());
            BlockPos.MutableBlockPos pos = new BlockPos.MutableBlockPos(boundingBox.minX(), top, boundingBox.minZ());
            for (; pos.getY() > level.getMinY(); pos.move(0, -1, 0)) {
                BlockState here = level.getBlockState(pos);
                BlockState under = level.getBlockState(pos.below());
                if (!(under.is(Blocks.SANDSTONE) || under.is(Blocks.STONE) || under.is(Blocks.ANDESITE) || under.is(Blocks.GRANITE) || under.is(Blocks.DIORITE))) {
                    continue;
                }
                BlockState filler = here.isAir() || wet(here) ? Blocks.SAND.defaultBlockState() : here;
                for (Direction side : Direction.values()) {
                    BlockPos next = pos.relative(side);
                    BlockState found = level.getBlockState(next);
                    if (found.isAir() || wet(found)) {
                        BlockState beneath = level.getBlockState(next.below());
                        boolean hollow = beneath.isAir() || wet(beneath);
                        level.setBlockAndUpdate(next, hollow && side != Direction.UP ? under : filler);
                    }
                }
                boundingBox = new BoundingBox(pos);
                createChest(level, chunkBB, random, pos, BuiltInLootTables.BURIED_TREASURE, null);
                return;
            }
        }
    }
}
