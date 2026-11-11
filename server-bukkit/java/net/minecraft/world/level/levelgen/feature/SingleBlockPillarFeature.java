// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.levelgen.feature;
import com.mojang.serialization.MapCodec;
import java.util.Optional;
import net.minecraft.core.BlockPos;
import net.minecraft.core.Direction;
import net.minecraft.core.Holder;
import net.minecraft.util.RandomSource;
import net.minecraft.world.level.WorldGenLevel;
import net.minecraft.world.level.chunk.ChunkGenerator;
import net.minecraft.world.level.levelgen.blockpredicates.BlockPredicate;
import net.minecraft.world.level.levelgen.feature.stateproviders.BlockStateProvider;
import net.minecraft.world.level.levelgen.placement.PlacedFeature;
public record SingleBlockPillarFeature(Holder<BlockStateProvider> block, BlockPredicate canReplace, Direction direction, float chanceToContinue, Optional<Holder<PlacedFeature>> capFeature) implements Feature {
    public static final MapCodec<SingleBlockPillarFeature> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(net.minecraft.world.level.levelgen.feature.SingleBlockPillarFeature.class, "CODEC");
    public SingleBlockPillarFeature(Holder<BlockStateProvider> block, BlockPredicate mayReplace, Direction direction, float chanceToContinue) {
        this(block, mayReplace, direction, chanceToContinue, Optional.empty());
    }
    public MapCodec<SingleBlockPillarFeature> codec() {
        return CODEC;
    }
    public boolean place(WorldGenLevel level, ChunkGenerator chunkGenerator, RandomSource random, BlockPos origin) {
        BlockPos.MutableBlockPos cursor = origin.mutable();
        while (canReplace.test(level, cursor) && random.nextFloat() < chanceToContinue && !level.isOutsideBuildHeight(cursor)) {
            level.setBlock(cursor, block.value().getState(level, random, cursor), 2);
            cursor.move(direction);
        }
        cursor.move(direction.getOpposite());
        capFeature.ifPresent(cap -> cap.value().place(level, chunkGenerator, random, cursor));
        return true;
    }
}
