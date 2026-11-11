// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.levelgen.feature.foliageplacers;
import com.mojang.serialization.MapCodec;
public class FoliagePlacerType<P extends FoliagePlacer> {
    private final MapCodec<P> codec;
    public static final FoliagePlacerType<BlobFoliagePlacer> BLOB_FOLIAGE_PLACER = new FoliagePlacerType<>(BlobFoliagePlacer.CODEC);
    public static final FoliagePlacerType<SpruceFoliagePlacer> SPRUCE_FOLIAGE_PLACER = new FoliagePlacerType<>(SpruceFoliagePlacer.CODEC);
    public static final FoliagePlacerType<PineFoliagePlacer> PINE_FOLIAGE_PLACER = new FoliagePlacerType<>(PineFoliagePlacer.CODEC);
    public static final FoliagePlacerType<AcaciaFoliagePlacer> ACACIA_FOLIAGE_PLACER = new FoliagePlacerType<>(AcaciaFoliagePlacer.CODEC);
    public static final FoliagePlacerType<BushFoliagePlacer> BUSH_FOLIAGE_PLACER = new FoliagePlacerType<>(BushFoliagePlacer.CODEC);
    public static final FoliagePlacerType<FancyFoliagePlacer> FANCY_FOLIAGE_PLACER = new FoliagePlacerType<>(FancyFoliagePlacer.CODEC);
    public static final FoliagePlacerType<MegaJungleFoliagePlacer> MEGA_JUNGLE_FOLIAGE_PLACER = new FoliagePlacerType<>(MegaJungleFoliagePlacer.CODEC);
    public static final FoliagePlacerType<MegaPineFoliagePlacer> MEGA_PINE_FOLIAGE_PLACER = new FoliagePlacerType<>(MegaPineFoliagePlacer.CODEC);
    public static final FoliagePlacerType<DarkOakFoliagePlacer> DARK_OAK_FOLIAGE_PLACER = new FoliagePlacerType<>(DarkOakFoliagePlacer.CODEC);
    public static final FoliagePlacerType<RandomSpreadFoliagePlacer> RANDOM_SPREAD_FOLIAGE_PLACER = new FoliagePlacerType<>(RandomSpreadFoliagePlacer.CODEC);
    public static final FoliagePlacerType<CherryFoliagePlacer> CHERRY_FOLIAGE_PLACER = new FoliagePlacerType<>(CherryFoliagePlacer.CODEC);
    public static final FoliagePlacerType<PoplarFoliagePlacer> POPLAR_FOLIAGE_PLACER = new FoliagePlacerType<>(PoplarFoliagePlacer.CODEC);
    private FoliagePlacerType(MapCodec<P> codec) {
        this.codec = codec;
    }
    public MapCodec<P> codec() {
        return codec;
    }
}
