// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.scores;
import net.minecraft.network.chat.Component;
import net.minecraft.network.chat.MutableComponent;
import net.minecraft.network.chat.numbers.NumberFormat;
public record PlayerScoreEntry(String owner, int value, Component display, NumberFormat numberFormatOverride) {
    public boolean isHidden() {
        return owner.startsWith("#");
    }
    public Component ownerName() {
        return Component.literal(owner);
    }
    public MutableComponent formatValue(NumberFormat _default) {
        return (numberFormatOverride != null ? numberFormatOverride : _default).format(value);
    }
}
