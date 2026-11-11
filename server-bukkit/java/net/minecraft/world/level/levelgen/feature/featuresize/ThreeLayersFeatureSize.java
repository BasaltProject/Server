// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.levelgen.feature.featuresize;
import com.mojang.serialization.MapCodec;
import java.util.OptionalInt;
public class ThreeLayersFeatureSize extends FeatureSize {
    private final int limit;
    private final int upperLimit;
    private final int lowerSize;
    private final int middleSize;
    private final int upperSize;
    public static final MapCodec<ThreeLayersFeatureSize> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(net.minecraft.world.level.levelgen.feature.featuresize.ThreeLayersFeatureSize.class, "CODEC");
    public ThreeLayersFeatureSize(int limit, int upperLimit, int lowerSize, int middleSize, int upperSize, OptionalInt minClippedHeight) {
        super(minClippedHeight);
        this.limit = limit;
        this.upperLimit = upperLimit;
        this.lowerSize = lowerSize;
        this.middleSize = middleSize;
        this.upperSize = upperSize;
    }
    protected FeatureSizeType<?> type() {
        return FeatureSizeType.THREE_LAYERS_FEATURE_SIZE;
    }
    public int getSizeAtHeight(int treeHeight, int yo) {
        if (yo < limit) {
            return lowerSize;
        }
        return yo >= treeHeight - upperLimit ? upperSize : middleSize;
    }
}
