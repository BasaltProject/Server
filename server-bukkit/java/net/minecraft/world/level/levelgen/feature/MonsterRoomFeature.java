// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.levelgen.feature;
import net.minecraft.world.level.block.Blocks;
import net.minecraft.world.level.block.state.BlockState;
import net.minecraft.core.Direction;
import net.minecraft.tags.BlockTags;
import java.util.function.Predicate;
import net.minecraft.world.entity.EntityType;
import net.minecraft.world.entity.EntityTypes;
import net.minecraft.world.level.block.entity.SpawnerBlockEntity;
import net.minecraft.world.level.storage.loot.BuiltInLootTables;
import net.minecraft.world.RandomizableContainer;
import net.minecraft.world.level.levelgen.structure.StructurePiece;
import com.mojang.serialization.MapCodec;
import net.minecraft.core.BlockPos;
import net.minecraft.util.RandomSource;
import net.minecraft.world.level.WorldGenLevel;
import net.minecraft.world.level.chunk.ChunkGenerator;
public record MonsterRoomFeature() implements Feature {
    private static final EntityType<?>[] kMobs = {EntityTypes.SKELETON, EntityTypes.ZOMBIE, EntityTypes.ZOMBIE, EntityTypes.SPIDER};
    public static final MapCodec<MonsterRoomFeature> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(net.minecraft.world.level.levelgen.feature.MonsterRoomFeature.class, "CODEC");
    public MapCodec<MonsterRoomFeature> codec() {
        return CODEC;
    }
    public boolean place(WorldGenLevel level, ChunkGenerator chunkGenerator, RandomSource random, BlockPos origin) {
        Predicate<BlockState> free = s -> !s.is(BlockTags.FEATURES_CANNOT_REPLACE);
        BlockState cave = Blocks.CAVE_AIR.defaultBlockState();
        int halfX = random.nextInt(2) + 2;
        int halfZ = random.nextInt(2) + 2;
        int loX = -halfX - 1;
        int hiX = halfX + 1;
        int loZ = -halfZ - 1;
        int hiZ = halfZ + 1;
        int doors = 0;
        for (int dx = loX; dx <= hiX; dx++) {
            for (int dy = -1; dy <= 4; dy++) {
                for (int dz = loZ; dz <= hiZ; dz++) {
                    BlockPos at = origin.offset(dx, dy, dz);
                    boolean solid = level.getBlockState(at).isSolid();
                    if ((dy == -1 || dy == 4) && !solid) {
                        return false;
                    }
                    boolean rim = dx == loX || dx == hiX || dz == loZ || dz == hiZ;
                    if (rim && dy == 0 && level.isEmptyBlock(at) && level.isEmptyBlock(at.above())) {
                        doors++;
                    }
                }
            }
        }
        if (doors < 1 || doors > 5) {
            return false;
        }
        for (int dx = loX; dx <= hiX; dx++) {
            for (int dy = 3; dy >= -1; dy--) {
                for (int dz = loZ; dz <= hiZ; dz++) {
                    BlockPos at = origin.offset(dx, dy, dz);
                    BlockState found = level.getBlockState(at);
                    boolean inside = dx != loX && dy != -1 && dz != loZ && dx != hiX && dy != 4 && dz != hiZ;
                    if (inside) {
                        if (!found.is(Blocks.CHEST) && !found.is(Blocks.SPAWNER)) {
                            safeSetBlock(level, at, cave, free);
                        }
                    } else if (at.getY() >= level.getMinY() && !level.getBlockState(at.below()).isSolid()) {
                        level.setBlock(at, cave, 2);
                    } else if (found.isSolid() && !found.is(Blocks.CHEST)) {
                        boolean mossy = dy == -1 && random.nextInt(4) != 0;
                        safeSetBlock(level, at, (mossy ? Blocks.MOSSY_COBBLESTONE : Blocks.COBBLESTONE).defaultBlockState(), free);
                    }
                }
            }
        }
        for (int chest = 0; chest < 2; chest++) {
            for (int attempt = 0; attempt < 3; attempt++) {
                BlockPos spot = new BlockPos(origin.getX() + random.nextInt(halfX * 2 + 1) - halfX, origin.getY(),
                    origin.getZ() + random.nextInt(halfZ * 2 + 1) - halfZ);
                if (!level.isEmptyBlock(spot)) {
                    continue;
                }
                int solidSides = 0;
                for (Direction side : Direction.Plane.HORIZONTAL) {
                    if (level.getBlockState(spot.relative(side)).isSolid()) {
                        solidSides++;
                    }
                }
                if (solidSides == 1) {
                    safeSetBlock(level, spot, StructurePiece.reorient(level, spot, Blocks.CHEST.defaultBlockState()), free);
                    if (level.getBlockEntity(spot) instanceof RandomizableContainer container) {
                        container.setLootTable(BuiltInLootTables.SIMPLE_DUNGEON, random.nextLong());
                    }
                    break;
                }
            }
        }
        safeSetBlock(level, origin, Blocks.SPAWNER.defaultBlockState(), free);
        if (level.getBlockEntity(origin) instanceof SpawnerBlockEntity spawner) {
            spawner.setEntityId(kMobs[random.nextInt(kMobs.length)], random);
        }
        return true;


    }
}
