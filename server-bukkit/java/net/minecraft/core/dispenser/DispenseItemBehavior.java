// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.core.dispenser;
import net.minecraft.world.item.ItemStack;
import org.slf4j.Logger;
public interface DispenseItemBehavior {
    Logger LOGGER = org.slf4j.LoggerFactory.getLogger(DispenseItemBehavior.class);
    DispenseItemBehavior NOOP = (source, dispensed) -> dispensed;
    ItemStack dispense(BlockSource source, ItemStack dispensed);
    static void bootStrap() {
        // the native dispenser owns the vanilla behaviours
    }
}
