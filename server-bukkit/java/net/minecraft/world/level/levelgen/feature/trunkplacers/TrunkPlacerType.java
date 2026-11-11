// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.levelgen.feature.trunkplacers;
import com.mojang.serialization.MapCodec;
public class TrunkPlacerType<P extends TrunkPlacer> {
    private final MapCodec<P> codec;
    public static final TrunkPlacerType<StraightTrunkPlacer> STRAIGHT_TRUNK_PLACER = new TrunkPlacerType<>(StraightTrunkPlacer.CODEC);
    public static final TrunkPlacerType<ForkingTrunkPlacer> FORKING_TRUNK_PLACER = new TrunkPlacerType<>(ForkingTrunkPlacer.CODEC);
    public static final TrunkPlacerType<GiantTrunkPlacer> GIANT_TRUNK_PLACER = new TrunkPlacerType<>(GiantTrunkPlacer.CODEC);
    public static final TrunkPlacerType<MegaJungleTrunkPlacer> MEGA_JUNGLE_TRUNK_PLACER = new TrunkPlacerType<>(MegaJungleTrunkPlacer.CODEC);
    public static final TrunkPlacerType<DarkOakTrunkPlacer> DARK_OAK_TRUNK_PLACER = new TrunkPlacerType<>(DarkOakTrunkPlacer.CODEC);
    public static final TrunkPlacerType<FancyTrunkPlacer> FANCY_TRUNK_PLACER = new TrunkPlacerType<>(FancyTrunkPlacer.CODEC);
    public static final TrunkPlacerType<BendingTrunkPlacer> BENDING_TRUNK_PLACER = new TrunkPlacerType<>(BendingTrunkPlacer.CODEC);
    public static final TrunkPlacerType<UpwardsBranchingTrunkPlacer> UPWARDS_BRANCHING_TRUNK_PLACER = new TrunkPlacerType<>(UpwardsBranchingTrunkPlacer.CODEC);
    public static final TrunkPlacerType<CherryTrunkPlacer> CHERRY_TRUNK_PLACER = new TrunkPlacerType<>(CherryTrunkPlacer.CODEC);
    public static final TrunkPlacerType<PoplarTrunkPlacer> POPLAR_TRUNK_PLACER = new TrunkPlacerType<>(PoplarTrunkPlacer.CODEC);
    private TrunkPlacerType(MapCodec<P> codec) {
        this.codec = codec;
    }
    public MapCodec<P> codec() {
        return codec;
    }
}
