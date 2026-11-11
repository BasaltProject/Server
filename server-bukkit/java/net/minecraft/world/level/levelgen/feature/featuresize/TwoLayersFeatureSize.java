// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.levelgen.feature.featuresize;
import com.mojang.serialization.MapCodec;
import java.util.OptionalInt;
public class TwoLayersFeatureSize extends FeatureSize {
    private final int limit;
    private final int lowerSize;
    private final int upperSize;
    public static final MapCodec<TwoLayersFeatureSize> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(net.minecraft.world.level.levelgen.feature.featuresize.TwoLayersFeatureSize.class, "CODEC");
    public TwoLayersFeatureSize(int limit, int lowerSize, int upperSize) {
        this(limit, lowerSize, upperSize, OptionalInt.empty());
    }
    public TwoLayersFeatureSize(int limit, int lowerSize, int upperSize, OptionalInt minClippedHeight) {
        super(minClippedHeight);
        this.limit = limit;
        this.lowerSize = lowerSize;
        this.upperSize = upperSize;
    }
    protected FeatureSizeType<?> type() {
        return FeatureSizeType.TWO_LAYERS_FEATURE_SIZE;
    }
    public int getSizeAtHeight(int treeHeight, int yo) {
        return yo < limit ? lowerSize : upperSize;
    }
}
