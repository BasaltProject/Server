// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.scores;

import io.netty.buffer.ByteBuf;
import java.util.List;
import net.minecraft.network.chat.TextColor;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.util.StringRepresentable;
public enum TeamColor implements StringRepresentable {
    BLACK,
    DARK_BLUE,
    DARK_GREEN,
    DARK_AQUA,
    DARK_RED,
    DARK_PURPLE,
    GOLD,
    GRAY,
    DARK_GRAY,
    BLUE,
    GREEN,
    AQUA,
    RED,
    LIGHT_PURPLE,
    YELLOW,
    WHITE;
    private static final int[] kRgb = {0x000000, 0x0000AA, 0x00AA00, 0x00AAAA, 0xAA0000, 0xAA00AA, 0xFFAA00, 0xAAAAAA,
        0x555555, 0x5555FF, 0x55FF55, 0x55FFFF, 0xFF5555, 0xFF55FF, 0xFFFF55, 0xFFFFFF};
    public int id() {
        return ordinal();
    }
    public String getSerializedName() {
        return name().toLowerCase(java.util.Locale.ROOT);
    }
    public static TeamColor byName(String name) {
        if (name == null)
            return null;
        try {
            return valueOf(name.toUpperCase(java.util.Locale.ROOT));
        } catch (IllegalArgumentException e) {
            return null;
        }
    }
    public static final List<TeamColor> VALUES = List.of(values());
    public static final StringRepresentable.EnumCodec<TeamColor> CODEC = StringRepresentable.fromEnum(TeamColor::values);
    public static final StreamCodec<ByteBuf, TeamColor> STREAM_CODEC = net.minecraft.network.codec.ByteBufCodecs.idMapper(i -> values()[i], TeamColor::id);
    public TextColor textColor() {
        return net.minecraft.network.chat.TextColor.fromRgb(rgb());
    }
    public DisplaySlot displaySlot() {
        return DisplaySlot.valueOf("TEAM_" + name());
    }
    public int rgb() {
        return kRgb[ordinal()];
    }
}
