// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity;
import net.minecraft.world.item.ItemStack;
public class Crackiness {
    private final float fractionLow;
    private final float fractionMedium;
    private final float fractionHigh;
    public static final Crackiness GOLEM = new Crackiness(0.75F, 0.5F, 0.25F);
    public static final Crackiness WOLF_ARMOR = new Crackiness(0.95F, 0.69F, 0.32F);
    private Crackiness(float fractionLow, float fractionMedium, float fractionHigh) {
        this.fractionLow = fractionLow;
        this.fractionMedium = fractionMedium;
        this.fractionHigh = fractionHigh;
    }
    public Level byFraction(float fraction) {
        if (fraction < fractionHigh) {
            return Level.HIGH;
        }
        if (fraction < fractionMedium) {
            return Level.MEDIUM;
        }
        return fraction < fractionLow ? Level.LOW : Level.NONE;
    }
    public Level byDamage(ItemStack item) {
        return item.isDamageableItem() ? byDamage(item.getDamageValue(), item.getMaxDamage()) : Level.NONE;
    }
    public Level byDamage(int damage, int maxDamage) {
        return byFraction((float) (maxDamage - damage) / (float) maxDamage);
    }
    public static enum Level {
        NONE, LOW, MEDIUM, HIGH;
    }
}
