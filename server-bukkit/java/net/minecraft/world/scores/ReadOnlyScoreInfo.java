// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.scores;
import net.minecraft.network.chat.MutableComponent;
import net.minecraft.network.chat.numbers.NumberFormat;
public interface ReadOnlyScoreInfo {
    int value();
    boolean isLocked();
    NumberFormat numberFormat();
    default MutableComponent formatValue(NumberFormat defaultFormat) {
        return (numberFormat() != null ? numberFormat() : defaultFormat).format(value());
    }
}
