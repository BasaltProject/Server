// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.scores;
import com.mojang.serialization.MapCodec;
import java.util.Optional;
import net.minecraft.network.chat.Component;
import net.minecraft.network.chat.numbers.NumberFormat;
public class Score implements ReadOnlyScoreInfo {
    private int value;
    private boolean locked = true;
    private Component display;
    private NumberFormat numberFormat;
    public Score() {

    }
    public Score(Packed packed) {
        value = packed.value();
        locked = packed.locked();
        display = packed.display().orElse(null);
        numberFormat = packed.numberFormat().orElse(null);
    }
    public Packed pack() {
        return new Packed(value, locked, Optional.ofNullable(display), Optional.ofNullable(numberFormat));
    }
    public int value() {
        return value;
    }
    public void value(int score) {
        this.value = score;
    }
    public boolean isLocked() {
        return locked;
    }
    public void setLocked(boolean locked) {
        this.locked = locked;
    }
    public Component display() {
        return display;
    }
    public void display(Component display) {
        this.display = display;
    }
    public NumberFormat numberFormat() {
        return numberFormat;
    }
    public void numberFormat(NumberFormat numberFormat) {
        this.numberFormat = numberFormat;
    }
    public static record Packed(int value, boolean locked, Optional<Component> display, Optional<NumberFormat> numberFormat) {
        public static final MapCodec<Packed> MAP_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(net.minecraft.world.scores.Score.Packed.class, "MAP_CODEC");
    }
}
