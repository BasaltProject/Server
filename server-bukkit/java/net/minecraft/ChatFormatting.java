package net.minecraft;
public enum ChatFormatting {
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
    WHITE,
    OBFUSCATED,
    BOLD,
    STRIKETHROUGH,
    UNDERLINE,
    ITALIC,
    RESET;
    public int getId() {
        return ordinal() < 16 ? ordinal() : -1;
    }
    public boolean isColor() {
        return ordinal() < 16;
    }
    public boolean isFormat() {
        return ordinal() >= 16 && this != RESET;
    }
    public char getChar() {
        return "0123456789abcdefklmnor".charAt(ordinal());
    }
    @Override
    public String toString() {
        return "\u00a7" + getChar();
    }
    public static ChatFormatting getById(int id) {
        return id < 0 ? RESET : id < 16 ? values()[id] : null;
    }
    public static ChatFormatting getByName(String name) {
        if (name == null)
            return null;
        try {
            return valueOf(name.toUpperCase(java.util.Locale.ROOT));
        } catch (IllegalArgumentException e) {
            return null;
        }
    }
}
