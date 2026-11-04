// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.util;

import java.util.Locale;
import java.util.regex.Pattern;

public class StringUtil {
    private static final Pattern kColorCode = Pattern.compile("(?i)\\u00A7[0-9A-FK-OR]");
    private static final Pattern kLineBreak = Pattern.compile("\\r\\n|\\v");
    private static final Pattern kTrailingBreak = Pattern.compile("(?:\\r\\n|\\v)$");

    public static String formatTickDuration(int ticks, float tickrate) {
        int totalSeconds = Mth.floor((float) ticks / tickrate);
        int hours = totalSeconds / 3600;
        int minutes = totalSeconds / 60 % 60;
        int seconds = totalSeconds % 60;
        return hours > 0 ? String.format(Locale.ROOT, "%02d:%02d:%02d", hours, minutes, seconds)
            : String.format(Locale.ROOT, "%02d:%02d", minutes, seconds);
    }

    public static String stripColor(String input) {
        return kColorCode.matcher(input).replaceAll("");
    }

    public static boolean isNullOrEmpty(String s) {
        return s == null || s.isEmpty();
    }

    public static String truncateStringIfNecessary(String s, int maxLength, boolean addDotDotDotIfTruncated) {
        if (s.length() <= maxLength) {
            return s;
        }
        return addDotDotDotIfTruncated && maxLength > 3 ? s.substring(0, maxLength - 3) + "..." : s.substring(0, maxLength);
    }

    public static int lineCount(String s) {
        if (s.isEmpty()) {
            return 0;
        }
        int lines = 1;
        var matcher = kLineBreak.matcher(s);
        while (matcher.find()) {
            lines++;
        }
        return lines;
    }

    public static boolean endsWithNewLine(String s) {
        return kTrailingBreak.matcher(s).find();
    }

    public static String trimChatMessage(String message) {
        return truncateStringIfNecessary(message, 256, false);
    }

    public static boolean isAllowedChatCharacter(int ch) {
        return ch != 167 && ch >= 32 && ch != 127;
    }

    public static boolean isValidPlayerName(String name) {
        return name.length() <= 16 && name.chars().noneMatch(c -> c <= 32 || c >= 127);
    }

    public static String filterText(String input) {
        return filterText(input, false);
    }

    public static String filterText(String input, boolean multiline) {
        StringBuilder kept = new StringBuilder();
        for (char c : input.toCharArray()) {
            if (isAllowedChatCharacter(c) || multiline && c == '\n') {
                kept.append(c);
            }
        }
        return kept.toString();
    }

    public static boolean isWhitespace(int codepoint) {
        return Character.isWhitespace(codepoint) || Character.isSpaceChar(codepoint);
    }

    public static boolean isBlank(String string) {
        return string == null || string.isEmpty() || string.chars().allMatch(StringUtil::isWhitespace);
    }
}
