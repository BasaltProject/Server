// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.item.enchantment;
import net.minecraft.core.Holder;
public record EnchantmentInstance(Holder<Enchantment> enchantment, int level) {
    public int weight() {
        return enchantment.value().getWeight();
    }
}
