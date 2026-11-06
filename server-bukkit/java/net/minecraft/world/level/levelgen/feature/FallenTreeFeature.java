// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.levelgen.feature;

import java.util.List;
import net.minecraft.core.Holder;
import com.mojang.serialization.MapCodec;
import net.minecraft.core.BlockPos;
import net.minecraft.util.RandomSource;
import net.minecraft.util.valueproviders.IntProvider;
import net.minecraft.world.level.WorldGenLevel;
import net.minecraft.world.level.chunk.ChunkGenerator;
import net.minecraft.world.level.levelgen.feature.stateproviders.BlockStateProvider;
import net.minecraft.world.level.levelgen.feature.treedecorators.TreeDecorator;
public record FallenTreeFeature(Holder trunkProvider, Object logLength, List stumpDecorators, List logDecorators) implements Feature {
    public static final MapCodec<FallenTreeFeature> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(net.minecraft.world.level.levelgen.feature.FallenTreeFeature.class, "CODEC");
    public MapCodec<FallenTreeFeature> codec() {
        return CODEC;
    }
    public boolean place(WorldGenLevel level, ChunkGenerator chunkGenerator, RandomSource random, BlockPos origin) {
        net.minecraft.world.level.levelgen.feature.stateproviders.BlockStateProvider provider =
            (net.minecraft.world.level.levelgen.feature.stateproviders.BlockStateProvider) trunkProvider.value();
        String wood = org.basaltmc.bukkit.facade.NativeFeatures.woodOf(provider.getState(level, random, origin));
        return wood != null && switch (wood) {
            case "oak", "birch", "spruce", "jungle" -> org.basaltmc.bukkit.facade.NativeFeatures.placeTree(level, "fallen_" + wood, origin);
            default -> false;
        };
    }
    public static Builder builder(BlockStateProvider trunkProvider, IntProvider logLength) {
        return new Builder(trunkProvider, logLength);
    }
    public static class Builder {
        private final BlockStateProvider trunkProvider;
        private final IntProvider logLength;
        private final List<TreeDecorator> stumpDecorators = new java.util.ArrayList<>();
        private final List<TreeDecorator> logDecorators = new java.util.ArrayList<>();
        public Builder(BlockStateProvider trunkProvider, IntProvider logLength) {
            this.trunkProvider = trunkProvider;
            this.logLength = logLength;
        }
        public Builder stumpDecorator(TreeDecorator stumpDecorator) {
            stumpDecorators.add(stumpDecorator);
            return this;
        }
        public Builder logDecorator(TreeDecorator logDecorator) {
            logDecorators.add(logDecorator);
            return this;
        }
        public FallenTreeFeature build() {
            return new FallenTreeFeature(Holder.direct(trunkProvider), logLength, List.copyOf(stumpDecorators), List.copyOf(logDecorators));
        }
    }
}
