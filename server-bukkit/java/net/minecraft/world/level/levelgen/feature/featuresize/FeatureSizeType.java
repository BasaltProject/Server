// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.levelgen.feature.featuresize;
import com.mojang.serialization.MapCodec;
public class FeatureSizeType<P extends FeatureSize> {
    private final MapCodec<P> codec;
    public static final FeatureSizeType<TwoLayersFeatureSize> TWO_LAYERS_FEATURE_SIZE = new FeatureSizeType<>(TwoLayersFeatureSize.CODEC);
    public static final FeatureSizeType<ThreeLayersFeatureSize> THREE_LAYERS_FEATURE_SIZE = new FeatureSizeType<>(ThreeLayersFeatureSize.CODEC);
    private FeatureSizeType(MapCodec<P> codec) {
        this.codec = codec;
    }
    public MapCodec<P> codec() {
        return codec;
    }
}
