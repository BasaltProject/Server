// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.levelgen.densityfunction;
import net.minecraft.util.Mth;
import com.mojang.serialization.Codec;
import net.minecraft.util.StringRepresentable;
public enum DistanceMetric implements StringRepresentable {
    EUCLIDEAN, EUCLIDEAN_SQUARED, MANHATTAN, CHEBYSHEV;
    public static final Codec<DistanceMetric> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.codec(net.minecraft.world.level.levelgen.densityfunction.DistanceMetric.class, "CODEC");
    public float compute(float deltaX, float deltaY, float deltaZ) {
        return switch (this) {
            case EUCLIDEAN -> Mth.sqrt(deltaX * deltaX + deltaY * deltaY + deltaZ * deltaZ);
            case EUCLIDEAN_SQUARED -> deltaX * deltaX + deltaY * deltaY + deltaZ * deltaZ;
            case MANHATTAN -> Math.abs(deltaX) + Math.abs(deltaY) + Math.abs(deltaZ);
            case CHEBYSHEV -> Math.max(Math.max(Math.abs(deltaX), Math.abs(deltaY)), Math.abs(deltaZ));
        };
    }
    public float compute(float deltaX, float deltaZ) {
        return switch (this) {
            case EUCLIDEAN -> Mth.sqrt(deltaX * deltaX + deltaZ * deltaZ);
            case EUCLIDEAN_SQUARED -> deltaX * deltaX + deltaZ * deltaZ;
            case MANHATTAN -> Math.abs(deltaX) + Math.abs(deltaZ);
            case CHEBYSHEV -> Math.max(Math.abs(deltaX), Math.abs(deltaZ));
        };
    }
    public String getSerializedName() {
        return name().toLowerCase(java.util.Locale.ROOT);
    }
}
