// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.util;
import com.mojang.serialization.Codec;
public record Brightness(int block, int sky) {
    public static final Codec<Integer> LIGHT_VALUE_CODEC = ExtraCodecs.intRange(0, 15);
    public static final Codec<Brightness> CODEC = com.mojang.serialization.codecs.RecordCodecBuilder.create(i -> i.group(LIGHT_VALUE_CODEC.fieldOf("block").forGetter(Brightness::block), LIGHT_VALUE_CODEC.fieldOf("sky").forGetter(Brightness::sky)).apply(i, Brightness::new));
    public static final Brightness FULL_BRIGHT = new Brightness(15, 15);
    public int pack() {
        return block << 4 | sky << 20;
    }
    public static Brightness unpack(int packed) {
        return new Brightness(packed >> 4 & 15, packed >> 20 & 15);
    }
}
