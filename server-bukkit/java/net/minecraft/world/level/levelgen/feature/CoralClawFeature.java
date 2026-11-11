// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.levelgen.feature;
import net.minecraft.core.Direction;
import java.util.List;
import com.mojang.serialization.MapCodec;
import net.minecraft.core.BlockPos;
import net.minecraft.core.Holder;
import net.minecraft.util.RandomSource;
import net.minecraft.world.level.WorldGenLevel;
import net.minecraft.world.level.chunk.ChunkGenerator;
import net.minecraft.world.level.levelgen.placement.PlacedFeature;
public record CoralClawFeature(Holder<PlacedFeature> feature) implements Feature {
    public static final MapCodec<CoralClawFeature> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(net.minecraft.world.level.levelgen.feature.CoralClawFeature.class, "CODEC");
    public MapCodec<CoralClawFeature> codec() {
        return CODEC;
    }
    public boolean place(WorldGenLevel level, ChunkGenerator chunkGenerator, RandomSource random, BlockPos origin) {
        if (!feature.value().place(level, chunkGenerator, random, origin)) {
            return false;
        }
        Direction facing = Direction.Plane.HORIZONTAL.getRandomDirection(random);
        int branches = random.nextInt(2) + 2;
        List<Direction> options = new java.util.ArrayList<>(List.of(facing, facing.getClockWise(), facing.getCounterClockWise()));
        net.minecraft.util.Util.shuffle(options, random);
        for (Direction branch : options.subList(0, branches)) {
            BlockPos.MutableBlockPos cursor = origin.mutable();
            int sideLength = random.nextInt(2) + 1;
            cursor.move(branch);
            int forwardLength;
            Direction segment;
            if (branch == facing) {
                segment = facing;
                forwardLength = random.nextInt(3) + 2;
            } else {
                cursor.move(Direction.UP);
                Direction[] pair = {branch, Direction.UP};
                segment = pair[random.nextInt(pair.length)];
                forwardLength = random.nextInt(3) + 3;
            }
            for (int i = 0; i < sideLength && feature.value().place(level, chunkGenerator, random, cursor); i++) {
                cursor.move(segment);
            }
            cursor.move(segment.getOpposite());
            cursor.move(Direction.UP);
            for (int i = 0; i < forwardLength; i++) {
                cursor.move(facing);
                if (!feature.value().place(level, chunkGenerator, random, cursor)) {
                    break;
                }
                if (random.nextFloat() < 0.25F) {
                    cursor.move(Direction.UP);
                }
            }
        }
        return true;


    }
}
