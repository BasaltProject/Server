// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package org.basaltmc.bukkit.trade;

import java.util.Optional;
import net.minecraft.world.item.ItemStack;
import net.minecraft.world.item.trading.ItemCost;
import net.minecraft.world.item.trading.MerchantOffer;
import net.minecraft.world.item.trading.MerchantOffers;
import org.basaltmc.bukkit.Items;
import org.basaltmc.bukkit.MiscNatives;

/** Fills merchant offers from the native trade service. */
public final class TradeOffers {
    private TradeOffers() {
    }

    /** Rolls a profession's trade set at a level, or the listed trade ids when given, and appends the offers. */
    public static void add(MerchantOffers offers, String profession, int level, String world, double x, double y,
                           double z, String variant, int amount, long seed, String[] listed) {
        String[] flat = MiscNatives.tradeRoll(profession, level, world, x, y, z, variant, amount, seed, listed);
        for (int i = 0; flat != null && i + 11 < flat.length; i += 12) {
            ItemStack costA = stack(flat, i);
            ItemStack costB = stack(flat, i + 3);
            ItemStack result = stack(flat, i + 6);
            if (costA == null || result == null) {
                continue;
            }
            Optional<ItemCost> second = costB == null ? Optional.empty()
                : Optional.of(new ItemCost(costB.getItem(), costB.getCount()));
            offers.add(new MerchantOffer(new ItemCost(costA.getItem(), costA.getCount()), second, result,
                Integer.parseInt(flat[i + 9]), Integer.parseInt(flat[i + 10]), Float.parseFloat(flat[i + 11])));
        }
    }

    private static ItemStack stack(String[] flat, int at) {
        if (flat[at].isEmpty()) {
            return null;
        }
        StringBuilder text = new StringBuilder("{id:\"").append(flat[at]).append("\",count:").append(flat[at + 1]);
        if (!flat[at + 2].isEmpty()) {
            text.append(",components:").append(flat[at + 2]);
        }
        org.bukkit.inventory.ItemStack made = Items.fromSnbt(text.append('}').toString());
        return made == null ? null : org.bukkit.craftbukkit.inventory.CraftItemStack.asNMSCopy(made);
    }
}
