// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.levelgen.feature;
import net.minecraft.world.phys.shapes.VoxelShape;
import net.minecraft.world.phys.shapes.Shapes;
import net.minecraft.world.level.levelgen.Column;
import net.minecraft.world.level.block.Blocks;
import net.minecraft.world.level.block.Block;
import net.minecraft.world.level.block.state.BlockState;
import net.minecraft.core.Direction;
import net.minecraft.core.Vec3i;
import net.minecraft.world.level.LevelAccessor;
import net.minecraft.world.level.levelgen.structure.BoundingBox;
import java.util.OptionalInt;
import java.util.Optional;
import com.mojang.serialization.MapCodec;
import net.minecraft.core.BlockPos;
import net.minecraft.util.RandomSource;
import net.minecraft.world.level.WorldGenLevel;
import net.minecraft.world.level.chunk.ChunkGenerator;
public record UnderwaterMagmaFeature(int floorSearchRange, int placementRadiusAroundFloor, float placementProbabilityPerValidPosition) implements Feature {
    private static boolean wetOrEmpty(BlockState state) {
        return state.is(Blocks.WATER) || state.isAir();
    }
    private boolean exposed(LevelAccessor level, BlockPos pos, Direction covered) {
        VoxelShape shape = level.getBlockState(pos).getFaceOcclusionShape(covered);
        return shape == Shapes.empty() || !Block.isShapeFullBlock(shape);
    }
    private boolean suitable(WorldGenLevel level, BlockPos pos) {
        if (wetOrEmpty(level.getBlockState(pos)) || exposed(level, pos.below(), Direction.UP)) {
            return false;
        }
        for (Direction side : Direction.Plane.HORIZONTAL) {
            if (exposed(level, pos.relative(side), side.getOpposite())) {
                return false;
            }
        }
        return true;
    }
    public static final MapCodec<UnderwaterMagmaFeature> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(net.minecraft.world.level.levelgen.feature.UnderwaterMagmaFeature.class, "CODEC");
    public MapCodec<UnderwaterMagmaFeature> codec() {
        return CODEC;
    }
    public boolean place(WorldGenLevel level, ChunkGenerator chunkGenerator, RandomSource random, BlockPos origin) {
        Optional<Column> column = Column.scan(level, origin, floorSearchRange, s -> s.is(Blocks.WATER), s -> !s.is(Blocks.WATER));
        OptionalInt floor = column.map(Column::getFloor).orElseGet(OptionalInt::empty);
        if (floor.isEmpty()) {
            return false;
        }
        BlockPos base = origin.atY(floor.getAsInt());
        Vec3i reach = new Vec3i(placementRadiusAroundFloor, placementRadiusAroundFloor, placementRadiusAroundFloor);
        BoundingBox area = BoundingBox.fromCorners(base.subtract(reach), base.offset(reach));
        int placed = 0;
        for (BlockPos pos : (Iterable<BlockPos>) BlockPos.betweenClosedStream(area)::iterator) {
            if (random.nextFloat() < placementProbabilityPerValidPosition && suitable(level, pos)) {
                level.setBlock(pos, Blocks.MAGMA_BLOCK.defaultBlockState(), 2);
                placed++;
            }
        }
        return placed > 0;


    }
}
