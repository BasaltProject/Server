// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.util;
import com.mojang.serialization.Codec;
public record ColorRGBA(int rgba) {
    public static final Codec<ColorRGBA> CODEC = ExtraCodecs.STRING_ARGB_COLOR.xmap(ColorRGBA::new, ColorRGBA::rgba);
    public String toString() {
        return String.format("#%08X", rgba);
    }
}
