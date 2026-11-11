// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.levelgen.feature.featuresize;
import com.mojang.serialization.Codec;
import com.mojang.serialization.codecs.RecordCodecBuilder;
import java.util.OptionalInt;
public abstract class FeatureSize {
    public static final Codec<FeatureSize> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.codec(net.minecraft.world.level.levelgen.feature.featuresize.FeatureSize.class, "CODEC");
    protected static final int MAX_WIDTH = 16;
    protected final OptionalInt minClippedHeight;
    protected static <S extends FeatureSize> RecordCodecBuilder<S, OptionalInt> minClippedHeightCodec() {
        return Codec.intRange(0, 80).optionalFieldOf("min_clipped_height").xmap(o -> o.map(OptionalInt::of).orElse(OptionalInt.empty()),
            o -> o.isPresent() ? java.util.Optional.of(o.getAsInt()) : java.util.Optional.empty()).forGetter(f -> f.minClippedHeight);
    }
    public FeatureSize(OptionalInt minClippedHeight) {
        this.minClippedHeight = minClippedHeight;
    }
    protected abstract FeatureSizeType<?> type();
    public abstract int getSizeAtHeight(int treeHeight, int yo);
    public OptionalInt minClippedHeight() {
        return minClippedHeight;
    }
}
