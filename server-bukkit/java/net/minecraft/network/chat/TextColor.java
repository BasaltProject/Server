// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.network.chat;

import com.mojang.serialization.Codec;
import com.mojang.serialization.DataResult;
import java.util.HashMap;
import java.util.Map;
import java.util.Objects;
import net.minecraft.ChatFormatting;

public final class TextColor {
    private static final String CUSTOM_COLOR_PREFIX = "#";
    private static final Map<ChatFormatting, TextColor> LEGACY_FORMAT_TO_COLOR = new HashMap<>();
    private static final Map<String, TextColor> NAMED_COLORS = new HashMap<>();

    public static final TextColor BLACK = named(ChatFormatting.BLACK);
    public static final TextColor DARK_BLUE = named(ChatFormatting.DARK_BLUE);
    public static final TextColor DARK_GREEN = named(ChatFormatting.DARK_GREEN);
    public static final TextColor DARK_AQUA = named(ChatFormatting.DARK_AQUA);
    public static final TextColor DARK_RED = named(ChatFormatting.DARK_RED);
    public static final TextColor DARK_PURPLE = named(ChatFormatting.DARK_PURPLE);
    public static final TextColor GOLD = named(ChatFormatting.GOLD);
    public static final TextColor GRAY = named(ChatFormatting.GRAY);
    public static final TextColor DARK_GRAY = named(ChatFormatting.DARK_GRAY);
    public static final TextColor BLUE = named(ChatFormatting.BLUE);
    public static final TextColor GREEN = named(ChatFormatting.GREEN);
    public static final TextColor AQUA = named(ChatFormatting.AQUA);
    public static final TextColor RED = named(ChatFormatting.RED);
    public static final TextColor LIGHT_PURPLE = named(ChatFormatting.LIGHT_PURPLE);
    public static final TextColor YELLOW = named(ChatFormatting.YELLOW);
    public static final TextColor WHITE = named(ChatFormatting.WHITE);
    public static final Codec<TextColor> CODEC = Codec.STRING.comapFlatMap(TextColor::parseColor, TextColor::serialize);

    private final int value;
    private final String name;

    private TextColor(int value, String name) {
        this.value = value & 0xFFFFFF;
        this.name = name;
    }

    private TextColor(int value) {
        this(value, null);
    }

    private static TextColor named(ChatFormatting format) {
        TextColor color = new TextColor(format.getColor(), format.getName());
        LEGACY_FORMAT_TO_COLOR.put(format, color);
        NAMED_COLORS.put(format.getName(), color);
        return color;
    }

    public int getValue() {
        return value;
    }

    public String serialize() {
        return name != null ? name : formatValue();
    }

    private String formatValue() {
        return String.format("#%06X", value);
    }

    @Override
    public boolean equals(Object o) {
        return this == o || o instanceof TextColor other && value == other.value;
    }

    @Override
    public int hashCode() {
        return Objects.hash(value, name);
    }

    @Override
    public String toString() {
        return serialize();
    }

    public static TextColor fromLegacyFormat(ChatFormatting format) {
        return LEGACY_FORMAT_TO_COLOR.get(format);
    }

    public static TextColor fromRgb(int rgb) {
        return new TextColor(rgb);
    }

    public static DataResult<TextColor> parseColor(String color) {
        if (color.startsWith(CUSTOM_COLOR_PREFIX)) {
            try {
                int value = Integer.parseInt(color.substring(1), 16);
                return value >= 0 && value <= 0xFFFFFF ? DataResult.success(fromRgb(value))
                    : DataResult.error(() -> "Color value out of range: " + color);
            } catch (NumberFormatException e) {
                return DataResult.error(() -> "Invalid color value: " + color);
            }
        }
        TextColor named = NAMED_COLORS.get(color);
        return named == null ? DataResult.error(() -> "Invalid color name: " + color) : DataResult.success(named);
    }
}
