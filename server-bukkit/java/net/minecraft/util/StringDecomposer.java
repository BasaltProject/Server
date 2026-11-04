// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.util;
import java.util.Optional;
import net.minecraft.ChatFormatting;
import net.minecraft.network.chat.FormattedText;
import net.minecraft.network.chat.Style;
public class StringDecomposer {
    private static final char kReplacement = '�';
    private static boolean feedChar(Style style, FormattedCharSink output, int position, char c) {
        return Character.isSurrogate(c) ? output.accept(position, style, kReplacement) : output.accept(position, style, c);
    }
    public static boolean iterate(String string, Style style, FormattedCharSink output) {
        int length = string.length();
        for (int i = 0; i < length; i++) {
            char c = string.charAt(i);
            if (Character.isHighSurrogate(c)) {
                if (i + 1 >= length) {
                    return output.accept(i, style, kReplacement) && true;
                }
                char next = string.charAt(i + 1);
                boolean ok = Character.isLowSurrogate(next) ? output.accept(i, style, Character.toCodePoint(c, next)) : output.accept(i, style, kReplacement);
                if (!ok) {
                    return false;
                }
                i++;
            } else if (!feedChar(style, output, i, c)) {
                return false;
            }
        }
        return true;
    }
    public static boolean iterateBackwards(String string, Style style, FormattedCharSink output) {
        int length = string.length();
        for (int i = length - 1; i >= 0; i--) {
            char c = string.charAt(i);
            if (Character.isLowSurrogate(c)) {
                if (i - 1 < 0) {
                    return output.accept(0, style, kReplacement);
                }
                char previous = string.charAt(i - 1);
                boolean ok = Character.isHighSurrogate(previous) ? output.accept(i - 1, style, Character.toCodePoint(previous, c)) : output.accept(i, style, kReplacement);
                if (!ok) {
                    return false;
                }
                i--;
            } else if (!feedChar(style, output, i, c)) {
                return false;
            }
        }
        return true;
    }
    public static boolean iterateFormatted(String string, Style style, FormattedCharSink output) {
        return iterateFormatted(string, 0, style, output);
    }
    public static boolean iterateFormatted(String string, int offset, Style style, FormattedCharSink output) {
        return iterateFormatted(string, offset, style, style, output);
    }
    public static boolean iterateFormatted(String string, int offset, Style currentStyle, Style resetStyle, FormattedCharSink output) {
        int length = string.length();
        Style style = currentStyle;
        for (int i = offset; i < length; i++) {
            char c = string.charAt(i);
            if (c == ChatFormatting.PREFIX_CODE) {
                if (i + 1 >= length) {
                    break;
                }
                ChatFormatting format = ChatFormatting.getByCode(string.charAt(i + 1));
                if (format != null) {
                    style = format == ChatFormatting.RESET ? resetStyle : style.applyLegacyFormat(format);
                }
                i++;
            } else if (Character.isHighSurrogate(c)) {
                if (i + 1 >= length) {
                    if (!output.accept(i, style, kReplacement)) {
                        return false;
                    }
                    break;
                }
                char next = string.charAt(i + 1);
                boolean ok = Character.isLowSurrogate(next) ? output.accept(i, style, Character.toCodePoint(c, next)) : output.accept(i, style, kReplacement);
                if (!ok) {
                    return false;
                }
                i++;
            } else if (!feedChar(style, output, i, c)) {
                return false;
            }
        }
        return true;
    }
    public static boolean iterateFormatted(FormattedText component, Style rootStyle, FormattedCharSink output) {
        return component.<Boolean>visit((style, text) -> iterateFormatted(text, 0, style, output) ? Optional.empty() : Optional.of(Boolean.TRUE), rootStyle).isEmpty();
    }
    public static String filterBrokenSurrogates(String input) {
        StringBuilder out = new StringBuilder(input.length());
        iterate(input, Style.EMPTY, (position, style, codepoint) -> {
            out.appendCodePoint(codepoint);
            return true;
        });
        return out.toString();
    }
    public static String getPlainText(FormattedText input) {
        StringBuilder out = new StringBuilder();
        iterateFormatted(input, Style.EMPTY, (position, style, codepoint) -> {
            out.appendCodePoint(codepoint);
            return true;
        });
        return out.toString();
    }
}
