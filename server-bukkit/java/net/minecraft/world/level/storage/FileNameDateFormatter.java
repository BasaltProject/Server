// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.storage;
import java.time.format.DateTimeFormatter;
public class FileNameDateFormatter {
    public static final DateTimeFormatter FORMATTER = new java.time.format.DateTimeFormatterBuilder()
        .appendValue(java.time.temporal.ChronoField.YEAR, 4, 10, java.time.format.SignStyle.EXCEEDS_PAD).appendLiteral('-')
        .appendValue(java.time.temporal.ChronoField.MONTH_OF_YEAR, 2).appendLiteral('-')
        .appendValue(java.time.temporal.ChronoField.DAY_OF_MONTH, 2).appendLiteral('_')
        .appendValue(java.time.temporal.ChronoField.HOUR_OF_DAY, 2).appendLiteral('-')
        .appendValue(java.time.temporal.ChronoField.MINUTE_OF_HOUR, 2).appendLiteral('-')
        .appendValue(java.time.temporal.ChronoField.SECOND_OF_MINUTE, 2).toFormatter(java.util.Locale.ROOT);
}
