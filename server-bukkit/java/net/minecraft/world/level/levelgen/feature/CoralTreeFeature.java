// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.levelgen.feature;

import java.util.List;
import net.minecraft.core.Holder;
import com.mojang.serialization.MapCodec;
import net.minecraft.core.BlockPos;
import net.minecraft.util.RandomSource;
import net.minecraft.world.level.WorldGenLevel;
import net.minecraft.world.level.chunk.ChunkGenerator;
import net.minecraft.world.level.levelgen.placement.PlacedFeature;
public record CoralTreeFeature(Holder feature) implements Feature {
    public static final MapCodec<CoralTreeFeature> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(net.minecraft.world.level.levelgen.feature.CoralTreeFeature.class, "CODEC");
    public MapCodec<CoralTreeFeature> codec() {
        return CODEC;
    }
    public boolean place(WorldGenLevel level, ChunkGenerator chunkGenerator, RandomSource random, BlockPos origin) {
        PlacedFeature placed = (PlacedFeature) feature.value();
        BlockPos.MutableBlockPos cursor = origin.mutable();
        int trunkHeight = random.nextInt(3) + 1;
        for (int i = 0; i < trunkHeight; i++) {
            if (!placed.place(level, chunkGenerator, random, cursor)) {
                return true;
            }
            cursor.move(net.minecraft.core.Direction.UP);
        }
        BlockPos top = cursor.immutable();
        int branches = random.nextInt(3) + 2;
        List<net.minecraft.core.Direction> directions = net.minecraft.core.Direction.Plane.HORIZONTAL.shuffledCopy(random);
        for (net.minecraft.core.Direction direction : directions.subList(0, branches)) {
            cursor.set(top);
            cursor.move(direction);
            int branchHeight = random.nextInt(5) + 2;
            int segment = 0;
            for (int j = 0; j < branchHeight && placed.place(level, chunkGenerator, random, cursor); j++) {
                segment++;
                cursor.move(net.minecraft.core.Direction.UP);
                if (j == 0 || segment >= 2 && random.nextFloat() < 0.25F) {
                    cursor.move(direction);
                    segment = 0;
                }
            }
        }
        return true;
    }
}
