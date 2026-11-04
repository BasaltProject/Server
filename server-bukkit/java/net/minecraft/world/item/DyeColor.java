// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.item;

import com.mojang.serialization.Codec;
import io.netty.buffer.ByteBuf;
import java.util.List;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.util.StringRepresentable;
import net.minecraft.world.level.material.MapColor;

public enum DyeColor implements StringRepresentable {
    WHITE("white", 16383998, MapColor.SNOW, MapColor.TERRACOTTA_WHITE, 15790320, 16777215),
    ORANGE("orange", 16351261, MapColor.COLOR_ORANGE, MapColor.TERRACOTTA_ORANGE, 15435844, 16738335),
    MAGENTA("magenta", 13061821, MapColor.COLOR_MAGENTA, MapColor.TERRACOTTA_MAGENTA, 12801229, 16711935),
    LIGHT_BLUE("light_blue", 3847130, MapColor.COLOR_LIGHT_BLUE, MapColor.TERRACOTTA_LIGHT_BLUE, 6719955, 10141901),
    YELLOW("yellow", 16701501, MapColor.COLOR_YELLOW, MapColor.TERRACOTTA_YELLOW, 14602026, 16776960),
    LIME("lime", 8439583, MapColor.COLOR_LIGHT_GREEN, MapColor.TERRACOTTA_LIGHT_GREEN, 4312372, 12582656),
    PINK("pink", 15961002, MapColor.COLOR_PINK, MapColor.TERRACOTTA_PINK, 14188952, 16738740),
    GRAY("gray", 4673362, MapColor.COLOR_GRAY, MapColor.TERRACOTTA_GRAY, 4408131, 8421504),
    LIGHT_GRAY("light_gray", 10329495, MapColor.COLOR_LIGHT_GRAY, MapColor.TERRACOTTA_LIGHT_GRAY, 11250603, 13882323),
    CYAN("cyan", 1481884, MapColor.COLOR_CYAN, MapColor.TERRACOTTA_CYAN, 2651799, 65535),
    PURPLE("purple", 8991416, MapColor.COLOR_PURPLE, MapColor.TERRACOTTA_PURPLE, 8073150, 10494192),
    BLUE("blue", 3949738, MapColor.COLOR_BLUE, MapColor.TERRACOTTA_BLUE, 2437522, 255),
    BROWN("brown", 8606770, MapColor.COLOR_BROWN, MapColor.TERRACOTTA_BROWN, 5320730, 9127187),
    GREEN("green", 6192150, MapColor.COLOR_GREEN, MapColor.TERRACOTTA_GREEN, 3887386, 65280),
    RED("red", 11546150, MapColor.COLOR_RED, MapColor.TERRACOTTA_RED, 11743532, 16711680),
    BLACK("black", 1908001, MapColor.COLOR_BLACK, MapColor.TERRACOTTA_BLACK, 1973019, 0);

    public static final List<DyeColor> VALUES = List.of(values());
    public static final StringRepresentable.EnumCodec<DyeColor> CODEC = StringRepresentable.fromEnum(DyeColor::values);
    public static final StreamCodec<ByteBuf, DyeColor> STREAM_CODEC = net.minecraft.network.codec.ByteBufCodecs.idMapper(
        DyeColor::byId, DyeColor::getId);
    public static final Codec<DyeColor> LEGACY_ID_CODEC = Codec.BYTE.xmap(DyeColor::byId, color -> (byte) color.getId());

    private final String name;
    private final int textureDiffuseColor;
    private final MapColor mapColor;
    private final MapColor terracottaColor;
    private final int fireworkColor;
    private final int textColor;

    private DyeColor(String name, int texture, MapColor mapColor, MapColor terracottaColor, int firework, int text) {
        this.name = name;
        this.textureDiffuseColor = 0xFF000000 | texture;
        this.mapColor = mapColor;
        this.terracottaColor = terracottaColor;
        this.fireworkColor = firework;
        this.textColor = 0xFF000000 | text;
    }

    public int getId() {
        return ordinal();
    }

    public String getName() {
        return name;
    }

    public int getTextureDiffuseColor() {
        return textureDiffuseColor;
    }

    public MapColor getMapColor() {
        return mapColor;
    }

    public MapColor getTerracottaColor() {
        return terracottaColor;
    }

    public int getFireworkColor() {
        return fireworkColor;
    }

    public int getTextColor() {
        return textColor;
    }

    public static DyeColor byId(int id) {
        return id < 0 || id >= VALUES.size() ? WHITE : VALUES.get(id);
    }

    public static DyeColor byName(String name, DyeColor def) {
        for (DyeColor color : values()) {
            if (color.name.equals(name)) {
                return color;
            }
        }
        return def;
    }

    public static DyeColor byFireworkColor(int color) {
        for (DyeColor candidate : values()) {
            if (candidate.fireworkColor == color) {
                return candidate;
            }
        }
        return null;
    }

    @Override
    public String toString() {
        return name;
    }

    @Override
    public String getSerializedName() {
        return name;
    }

    public static DyeColor getMixedColor(ServerLevel level, DyeColor first, DyeColor second) {
        return level.getRandom().nextBoolean() ? first : second;
    }
}
