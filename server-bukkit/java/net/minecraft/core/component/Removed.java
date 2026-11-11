// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.core.component;
import com.mojang.serialization.Codec;
import com.mojang.serialization.MapCodec;
final class Removed {
    public static final Removed INSTANCE = new Removed();
    public static final Codec<Removed> CODEC = MapCodec.unitCodec(INSTANCE);
    private Removed() {
    }
    public String toString() {
        return "<removed>";
    }
    public static boolean isNotRemoved(Object value) {
        return value != INSTANCE;
    }
    public static boolean isRemoved(Object value) {
        return value == INSTANCE;
    }
    public static Object nullToRemoved(Object value) {
        return value == null ? INSTANCE : value;
    }
    public static Object removedToNull(Object value) {
        return value == INSTANCE ? null : value;
    }
}
