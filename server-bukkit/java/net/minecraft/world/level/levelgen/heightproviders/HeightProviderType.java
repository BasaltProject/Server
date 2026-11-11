// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.levelgen.heightproviders;

import com.mojang.serialization.MapCodec;

public interface HeightProviderType<P extends HeightProvider> {
    HeightProviderType<ConstantHeight> CONSTANT = () -> ConstantHeight.CODEC;
    HeightProviderType<UniformHeight> UNIFORM = () -> UniformHeight.CODEC;
    HeightProviderType<BiasedToBottomHeight> BIASED_TO_BOTTOM = () -> BiasedToBottomHeight.CODEC;
    HeightProviderType<VeryBiasedToBottomHeight> VERY_BIASED_TO_BOTTOM = () -> VeryBiasedToBottomHeight.CODEC;
    HeightProviderType<TrapezoidHeight> TRAPEZOID = () -> TrapezoidHeight.CODEC;
    HeightProviderType<WeightedListHeight> WEIGHTED_LIST = () -> WeightedListHeight.CODEC;

    MapCodec<P> codec();
}
