// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.levelgen.blockpredicates;
import com.mojang.serialization.MapCodec;
public interface BlockPredicateType<P extends BlockPredicate> {
    BlockPredicateType<MatchingBlocksPredicate> MATCHING_BLOCKS = of(MatchingBlocksPredicate.CODEC);
    BlockPredicateType<MatchingBlockTagPredicate> MATCHING_BLOCK_TAG = of(MatchingBlockTagPredicate.CODEC);
    BlockPredicateType<MatchingFluidsPredicate> MATCHING_FLUIDS = of(MatchingFluidsPredicate.CODEC);
    BlockPredicateType<MatchingBiomesPredicate> MATCHING_BIOMES = of(MatchingBiomesPredicate.CODEC);
    BlockPredicateType<HasSturdyFacePredicate> HAS_STURDY_FACE = of(HasSturdyFacePredicate.CODEC);
    BlockPredicateType<SolidPredicate> SOLID = of(SolidPredicate.CODEC);
    BlockPredicateType<ReplaceablePredicate> REPLACEABLE = of(ReplaceablePredicate.CODEC);
    BlockPredicateType<WouldSurvivePredicate> WOULD_SURVIVE = of(WouldSurvivePredicate.CODEC);
    BlockPredicateType<InsideWorldBoundsPredicate> INSIDE_WORLD_BOUNDS = of(InsideWorldBoundsPredicate.CODEC);
    BlockPredicateType<AnyOfPredicate> ANY_OF = of(AnyOfPredicate.CODEC);
    BlockPredicateType<AllOfPredicate> ALL_OF = of(AllOfPredicate.CODEC);
    BlockPredicateType<NotPredicate> NOT = of(NotPredicate.CODEC);
    BlockPredicateType<TrueBlockPredicate> TRUE = of(TrueBlockPredicate.CODEC);
    BlockPredicateType<UnobstructedPredicate> UNOBSTRUCTED = of(UnobstructedPredicate.CODEC);
    BlockPredicateType<HeightRangePredicate> HEIGHT_RANGE = of(HeightRangePredicate.CODEC);
    BlockPredicateType<VolumeMatchPredicate> VOLUME_MATCH = of(VolumeMatchPredicate.CODEC);
    MapCodec<P> codec();
    static <P extends BlockPredicate> BlockPredicateType<P> of(MapCodec<P> codec) {
        return () -> codec;
    }
}
