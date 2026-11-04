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
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.levelgen.feature.FallenTreeFeature#codec");
    }
    public boolean place(WorldGenLevel level, ChunkGenerator chunkGenerator, RandomSource random, BlockPos origin) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.levelgen.feature.FallenTreeFeature#place");
    }
    public static Builder builder(BlockStateProvider trunkProvider, IntProvider logLength) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.levelgen.feature.FallenTreeFeature#builder");
    }
    public static class Builder {
        public Builder(BlockStateProvider trunkProvider, IntProvider logLength) {
            throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.levelgen.feature.FallenTreeFeature.Builder#<init>");
        }
        public Builder stumpDecorator(TreeDecorator stumpDecorator) {
            throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.levelgen.feature.FallenTreeFeature.Builder#stumpDecorator");
        }
        public Builder logDecorator(TreeDecorator logDecorator) {
            throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.levelgen.feature.FallenTreeFeature.Builder#logDecorator");
        }
        public FallenTreeFeature build() {
            throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.levelgen.feature.FallenTreeFeature.Builder#build");
        }
    }
}
