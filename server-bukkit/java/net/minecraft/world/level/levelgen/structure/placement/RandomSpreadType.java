// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.levelgen.structure.placement;
import com.mojang.serialization.Codec;
import net.minecraft.util.RandomSource;
import net.minecraft.util.StringRepresentable;
public enum RandomSpreadType implements StringRepresentable {
    LINEAR, TRIANGULAR;
    public static final Codec<RandomSpreadType> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.codec(net.minecraft.world.level.levelgen.structure.placement.RandomSpreadType.class, "CODEC");
    public String getSerializedName() {
        return name().toLowerCase(java.util.Locale.ROOT);
    }
    public int evaluate(RandomSource random, int limit) {
        return this == LINEAR ? random.nextInt(limit) : (random.nextInt(limit) + random.nextInt(limit)) / 2;
    }
}
