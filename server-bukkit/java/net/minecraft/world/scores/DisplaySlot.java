// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.scores;

import java.util.function.IntFunction;
import net.minecraft.util.StringRepresentable;
public enum DisplaySlot implements StringRepresentable {
    LIST,
    SIDEBAR,
    BELOW_NAME,
    TEAM_BLACK,
    TEAM_DARK_BLUE,
    TEAM_DARK_GREEN,
    TEAM_DARK_AQUA,
    TEAM_DARK_RED,
    TEAM_DARK_PURPLE,
    TEAM_GOLD,
    TEAM_GRAY,
    TEAM_DARK_GRAY,
    TEAM_BLUE,
    TEAM_GREEN,
    TEAM_AQUA,
    TEAM_RED,
    TEAM_LIGHT_PURPLE,
    TEAM_YELLOW,
    TEAM_WHITE;
    public int id() {
        return ordinal();
    }
    public static final StringRepresentable.EnumCodec<DisplaySlot> CODEC = StringRepresentable.fromEnum(DisplaySlot::values);
    public static final IntFunction<DisplaySlot> BY_ID = i -> values()[Math.floorMod(i, values().length)];
    public String getSerializedName() {
        return name().toLowerCase(java.util.Locale.ROOT).replace("team_", "sidebar.team.");
    }
}
