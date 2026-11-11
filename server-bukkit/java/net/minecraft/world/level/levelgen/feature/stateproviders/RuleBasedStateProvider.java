// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.levelgen.feature.stateproviders;
import com.mojang.serialization.Codec;
import com.mojang.serialization.MapCodec;
import java.util.List;
import java.util.Optional;
import net.minecraft.core.BlockPos;
import net.minecraft.core.Holder;
import net.minecraft.util.RandomSource;
import net.minecraft.world.level.LevelAccessor;
import net.minecraft.world.level.block.Block;
import net.minecraft.world.level.block.state.BlockState;
import net.minecraft.world.level.levelgen.blockpredicates.BlockPredicate;
public record RuleBasedStateProvider(Holder<BlockStateProvider> fallback, List<Rule> rules) implements BlockStateProvider {
    public static final MapCodec<RuleBasedStateProvider> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(net.minecraft.world.level.levelgen.feature.stateproviders.RuleBasedStateProvider.class, "CODEC");
    private RuleBasedStateProvider(Optional<Holder<BlockStateProvider>> fallback, List<Rule> rules) {
        this(fallback.orElse(null), rules);
    }
    public static RuleBasedStateProvider ifTrueThenProvide(BlockPredicate ifTrue, Block thenProvide) {
        return ifTrueThenProvide(ifTrue, (BlockStateProvider) BlockStateProvider.of(thenProvide));
    }
    public static RuleBasedStateProvider ifTrueThenProvide(BlockPredicate ifTrue, BlockStateProvider thenProvide) {
        return new RuleBasedStateProvider((Holder<BlockStateProvider>) null, List.of(new Rule(ifTrue, Holder.direct(thenProvide))));
    }
    public MapCodec<RuleBasedStateProvider> codec() {
        return CODEC;
    }
    public BlockState getState(LevelAccessor level, RandomSource random, BlockPos pos) {
        BlockState found = getOptionalState(level, random, pos);
        return found != null ? found : level.getBlockState(pos);
    }
    public BlockState getOptionalState(LevelAccessor level, RandomSource random, BlockPos pos) {
        for (Rule rule : rules) {
            if (rule.ifTrue().test(level, pos)) {
                BlockState made = rule.then().value().getOptionalState(level, random, pos);
                if (made != null) {
                    return made;
                }
            }
        }
        return fallback == null ? null : fallback.value().getOptionalState(level, random, pos);
    }
    public static Builder builder() {
        return new Builder(null);
    }
    public static Builder builder(BlockStateProvider fallback) {
        return new Builder(fallback);
    }
    public static record Rule(BlockPredicate ifTrue, Holder<BlockStateProvider> then) {
        public static final Codec<Rule> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.codec(net.minecraft.world.level.levelgen.feature.stateproviders.RuleBasedStateProvider.Rule.class, "CODEC");
    }
    public static class Builder {
        private BlockStateProvider fallback;
        private final List<Rule> rules = new java.util.ArrayList<>();
        public Builder(BlockStateProvider fallback) {
        this.fallback = fallback;
        }
        public Builder ifTrueThenProvide(BlockPredicate ifTrue, BlockStateProvider thenProvide) {
            rules.add(new Rule(ifTrue, Holder.direct(thenProvide)));
            return this;
        }
        public Builder ifTrueThenProvide(BlockPredicate ifTrue, Block thenProvide) {
            rules.add(new Rule(ifTrue, BlockStateProvider.holderOf(thenProvide)));
            return this;
        }
        public Builder ifTrueThenProvide(BlockPredicate ifTrue, BlockState thenProvide) {
            rules.add(new Rule(ifTrue, BlockStateProvider.holderOf(thenProvide)));
            return this;
        }
        public RuleBasedStateProvider build() {
            return new RuleBasedStateProvider(fallback == null ? null : Holder.direct(fallback), rules);


        }
    }
}
