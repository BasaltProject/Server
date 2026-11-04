// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft;

import java.util.Collection;
import java.util.Locale;
import java.util.regex.Pattern;
import java.util.stream.Stream;
import com.mojang.serialization.Codec;
import net.minecraft.util.StringRepresentable;

public enum ChatFormatting implements StringRepresentable {
    BLACK("BLACK", '0', 0, 0x000000),
    DARK_BLUE("DARK_BLUE", '1', 1, 0x0000AA),
    DARK_GREEN("DARK_GREEN", '2', 2, 0x00AA00),
    DARK_AQUA("DARK_AQUA", '3', 3, 0x00AAAA),
    DARK_RED("DARK_RED", '4', 4, 0xAA0000),
    DARK_PURPLE("DARK_PURPLE", '5', 5, 0xAA00AA),
    GOLD("GOLD", '6', 6, 0xFFAA00),
    GRAY("GRAY", '7', 7, 0xAAAAAA),
    DARK_GRAY("DARK_GRAY", '8', 8, 0x555555),
    BLUE("BLUE", '9', 9, 0x5555FF),
    GREEN("GREEN", 'a', 10, 0x55FF55),
    AQUA("AQUA", 'b', 11, 0x55FFFF),
    RED("RED", 'c', 12, 0xFF5555),
    LIGHT_PURPLE("LIGHT_PURPLE", 'd', 13, 0xFF55FF),
    YELLOW("YELLOW", 'e', 14, 0xFFFF55),
    WHITE("WHITE", 'f', 15, 0xFFFFFF),
    OBFUSCATED("OBFUSCATED", 'k', true),
    BOLD("BOLD", 'l', true),
    STRIKETHROUGH("STRIKETHROUGH", 'm', true),
    UNDERLINE("UNDERLINE", 'n', true),
    ITALIC("ITALIC", 'o', true),
    RESET("RESET", 'r', -1, null);

    public static final char PREFIX_CODE = '§';
    private static final Pattern STRIP_FORMATTING = Pattern.compile("(?i)§[0-9A-FK-OR]");
    public static final Codec<ChatFormatting> CODEC = StringRepresentable.fromEnum(ChatFormatting::values);
    public static final Codec<ChatFormatting> COLOR_CODEC = CODEC.validate(
        format -> format.isFormat() ? com.mojang.serialization.DataResult.error(() -> "Formatting was not a valid color: " + format)
            : com.mojang.serialization.DataResult.success(format));

    private final String name;
    private final char code;
    private final boolean isFormat;
    private final String toString;
    private final int id;
    private final Integer color;

    ChatFormatting(String name, char code, int id, Integer color) {
        this(name, code, false, id, color);
    }

    ChatFormatting(String name, char code, boolean isFormat) {
        this(name, code, isFormat, -1, null);
    }

    ChatFormatting(String name, char code, boolean isFormat, int id, Integer color) {
        this.name = name;
        this.code = code;
        this.isFormat = isFormat;
        this.id = id;
        this.color = color;
        this.toString = PREFIX_CODE + String.valueOf(code);
    }

    public char getChar() {
        return code;
    }

    public int getId() {
        return id;
    }

    public boolean isFormat() {
        return isFormat;
    }

    public boolean isColor() {
        return !isFormat && this != RESET;
    }

    public Integer getColor() {
        return color;
    }

    public String getName() {
        return name().toLowerCase(Locale.ROOT);
    }

    @Override
    public String getSerializedName() {
        return getName();
    }

    @Override
    public String toString() {
        return toString;
    }

    public static String stripFormatting(String input) {
        return input == null ? null : STRIP_FORMATTING.matcher(input).replaceAll("");
    }

    public static ChatFormatting getByName(String name) {
        if (name == null) {
            return null;
        }
        try {
            return valueOf(name.toUpperCase(Locale.ROOT));
        } catch (IllegalArgumentException e) {
            return null;
        }
    }

    public static ChatFormatting getById(int id) {
        if (id < 0) {
            return RESET;
        }
        for (ChatFormatting format : values()) {
            if (format.getId() == id) {
                return format;
            }
        }
        return null;
    }

    public static ChatFormatting getByCode(char code) {
        char lower = Character.toLowerCase(code);
        for (ChatFormatting format : values()) {
            if (format.code == lower) {
                return format;
            }
        }
        return null;
    }

    public static Collection<String> getNames(boolean getColors, boolean getFormats) {
        return Stream.of(values()).filter(format -> (!format.isColor() || getColors) && (!format.isFormat() || getFormats))
            .map(ChatFormatting::getName).toList();
    }
}
