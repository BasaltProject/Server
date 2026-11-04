// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.util;
import com.mojang.serialization.Codec;
public enum TriState implements StringRepresentable {
    TRUE, FALSE, DEFAULT;
    public static final Codec<TriState> CODEC = StringRepresentable.fromEnum(TriState::values);
    public static TriState from(boolean value) {
        return value ? TRUE : FALSE;
    }
    public boolean toBoolean(boolean defaultValue) {
        return switch (this) {
            case TRUE -> true;
            case FALSE -> false;
            default -> defaultValue;
        };
    }
    public String getSerializedName() {
        return name().toLowerCase(java.util.Locale.ROOT);
    }
}
