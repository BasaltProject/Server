// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.stats;

import java.text.DecimalFormat;
import java.util.Locale;

public interface StatFormatter {
    DecimalFormat DECIMAL_FORMAT = new DecimalFormat("########0.00", java.text.DecimalFormatSymbols.getInstance(Locale.ROOT));
    StatFormatter DEFAULT = value -> java.text.NumberFormat.getIntegerInstance(Locale.US).format(value);
    StatFormatter DIVIDE_BY_TEN = value -> DECIMAL_FORMAT.format(value * 0.1);
    StatFormatter DISTANCE = value -> {
        double meters = value / 100.0;
        double kilometers = meters / 1000.0;
        if (kilometers > 0.5) {
            return DECIMAL_FORMAT.format(kilometers) + " km";
        }
        return meters > 0.5 ? DECIMAL_FORMAT.format(meters) + " m" : value + " cm";
    };
    StatFormatter TIME = value -> {
        double seconds = value / 20.0;
        double minutes = seconds / 60.0;
        double hours = minutes / 60.0;
        double days = hours / 24.0;
        double years = days / 365.0;
        if (years > 0.5) {
            return DECIMAL_FORMAT.format(years) + " y";
        }
        if (days > 0.5) {
            return DECIMAL_FORMAT.format(days) + " d";
        }
        if (hours > 0.5) {
            return DECIMAL_FORMAT.format(hours) + " h";
        }
        return minutes > 0.5 ? DECIMAL_FORMAT.format(minutes) + " m" : seconds + " s";
    };

    String format(int value);
}
