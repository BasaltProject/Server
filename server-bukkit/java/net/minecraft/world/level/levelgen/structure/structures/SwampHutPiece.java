// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.levelgen.structure.structures;
import net.minecraft.world.level.levelgen.structure.pieces.StructurePieceType;
import net.minecraft.world.entity.EntityTypes;
import net.minecraft.world.entity.EntityType;
import net.minecraft.world.entity.EntitySpawnReason;
import net.minecraft.world.entity.Mob;
import org.basaltmc.bukkit.facade.StateProps;
import net.minecraft.world.level.block.Blocks;
import net.minecraft.world.level.block.state.BlockState;
import net.minecraft.core.BlockPos;
import net.minecraft.nbt.CompoundTag;
import net.minecraft.util.RandomSource;
import net.minecraft.world.level.ChunkPos;
import net.minecraft.world.level.StructureManager;
import net.minecraft.world.level.WorldGenLevel;
import net.minecraft.world.level.chunk.ChunkGenerator;
import net.minecraft.world.level.levelgen.structure.BoundingBox;
import net.minecraft.world.level.levelgen.structure.ScatteredFeaturePiece;
import net.minecraft.world.level.levelgen.structure.pieces.StructurePieceSerializationContext;
public class SwampHutPiece extends ScatteredFeaturePiece {
    private boolean spawnedWitch;
    private boolean spawnedCat;
    private void box(WorldGenLevel level, BoundingBox chunkBB, int x0, int y0, int z0, int x1, int y1, int z1, BlockState state) {
        generateBox(level, chunkBB, x0, y0, z0, x1, y1, z1, state, state, false);
    }
    private static BlockState stairs(String facing) {
        return StateProps.with(Blocks.SPRUCE_STAIRS.defaultBlockState(), "facing", facing);
    }
    private void spawnInside(WorldGenLevel level, BoundingBox chunkBB, EntityType<?> type, boolean witch) {
        BlockPos pos = getWorldPos(2, 2, 5);
        if (!chunkBB.isInside(pos)) {
            return;
        }
        if (witch) {
            spawnedWitch = true;
        } else {
            spawnedCat = true;
        }
        if (type.create(level.getLevel(), EntitySpawnReason.STRUCTURE) instanceof Mob mob) {
            mob.setPersistenceRequired();
            mob.snapTo((double) pos.getX() + 0.5D, (double) pos.getY(), (double) pos.getZ() + 0.5D, 0.0F, 0.0F);
            mob.finalizeSpawn(level, level.getCurrentDifficultyAt(pos), EntitySpawnReason.STRUCTURE, null);
            level.addFreshEntityWithPassengers(mob);
        }
    }
    public SwampHutPiece(RandomSource random, int west, int north) {
        super(StructurePieceType.SWAMPLAND_HUT, west, 64, north, 7, 7, 9, getRandomHorizontalDirection(random));
    }
    public SwampHutPiece(CompoundTag tag) {
        super(StructurePieceType.SWAMPLAND_HUT, tag);
        this.spawnedWitch = tag.getBooleanOr("Witch", false);
        this.spawnedCat = tag.getBooleanOr("Cat", false);
    }
    protected void addAdditionalSaveData(StructurePieceSerializationContext context, CompoundTag tag) {
        super.addAdditionalSaveData(context, tag);
        tag.putBoolean("Witch", spawnedWitch);
        tag.putBoolean("Cat", spawnedCat);
    }
    public void postProcess(WorldGenLevel level, StructureManager structureManager, ChunkGenerator generator, RandomSource random, BoundingBox chunkBB, ChunkPos chunkPos, BlockPos referencePos) {
        if (!updateAverageGroundHeight(level, chunkBB, 0)) {
            return;
        }
        BlockState planks = Blocks.SPRUCE_PLANKS.defaultBlockState();
        BlockState log = Blocks.OAK_LOG.defaultBlockState();
        BlockState fence = Blocks.OAK_FENCE.defaultBlockState();
        BlockState air = Blocks.AIR.defaultBlockState();
        box(level, chunkBB, 1, 1, 1, 5, 1, 7, planks);
        box(level, chunkBB, 1, 4, 2, 5, 4, 7, planks);
        box(level, chunkBB, 2, 1, 0, 4, 1, 0, planks);
        box(level, chunkBB, 2, 2, 2, 3, 3, 2, planks);
        box(level, chunkBB, 1, 2, 3, 1, 3, 6, planks);
        box(level, chunkBB, 5, 2, 3, 5, 3, 6, planks);
        box(level, chunkBB, 2, 2, 7, 4, 3, 7, planks);
        box(level, chunkBB, 1, 0, 2, 1, 3, 2, log);
        box(level, chunkBB, 5, 0, 2, 5, 3, 2, log);
        box(level, chunkBB, 1, 0, 7, 1, 3, 7, log);
        box(level, chunkBB, 5, 0, 7, 5, 3, 7, log);
        placeBlock(level, fence, 2, 3, 2, chunkBB);
        placeBlock(level, fence, 3, 3, 7, chunkBB);
        placeBlock(level, air, 1, 3, 4, chunkBB);
        placeBlock(level, air, 5, 3, 4, chunkBB);
        placeBlock(level, air, 5, 3, 5, chunkBB);
        placeBlock(level, Blocks.POTTED_RED_MUSHROOM.defaultBlockState(), 1, 3, 5, chunkBB);
        placeBlock(level, Blocks.CRAFTING_TABLE.defaultBlockState(), 3, 2, 6, chunkBB);
        placeBlock(level, Blocks.CAULDRON.defaultBlockState(), 4, 2, 6, chunkBB);
        placeBlock(level, fence, 1, 2, 1, chunkBB);
        placeBlock(level, fence, 5, 2, 1, chunkBB);
        BlockState north = stairs("north");
        BlockState east = stairs("east");
        BlockState west = stairs("west");
        BlockState south = stairs("south");
        box(level, chunkBB, 0, 4, 1, 6, 4, 1, north);
        box(level, chunkBB, 0, 4, 2, 0, 4, 7, east);
        box(level, chunkBB, 6, 4, 2, 6, 4, 7, west);
        box(level, chunkBB, 0, 4, 8, 6, 4, 8, south);
        placeBlock(level, StateProps.with(north, "shape", "outer_right"), 0, 4, 1, chunkBB);
        placeBlock(level, StateProps.with(north, "shape", "outer_left"), 6, 4, 1, chunkBB);
        placeBlock(level, StateProps.with(south, "shape", "outer_left"), 0, 4, 8, chunkBB);
        placeBlock(level, StateProps.with(south, "shape", "outer_right"), 6, 4, 8, chunkBB);
        for (int z = 2; z <= 7; z += 5) {
            for (int x = 1; x <= 5; x += 4) {
                fillColumnDown(level, log, x, -1, z, chunkBB);
            }
        }
        if (!spawnedWitch) {
            spawnInside(level, chunkBB, EntityTypes.WITCH, true);
        }
        if (!spawnedCat) {
            spawnInside(level, chunkBB, EntityTypes.CAT, false);
        }


    }
}
