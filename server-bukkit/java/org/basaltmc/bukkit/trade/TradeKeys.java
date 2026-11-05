// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package org.basaltmc.bukkit.trade;

import java.util.Locale;
import java.util.regex.Matcher;
import java.util.regex.Pattern;
import net.minecraft.resources.Identifier;
import net.minecraft.resources.ResourceKey;
import net.minecraft.world.item.trading.TradeSet;
import net.minecraft.world.item.trading.VillagerTrade;

/** Registry keys of the villager trade and trade set files, derived from the constant names. */
public final class TradeKeys {
    private static final Pattern LEVELED = Pattern.compile("([A-Z]+(?:_[A-Z]+)*?)_(\\d)_(.*)");
    private static final Pattern SET = Pattern.compile("([A-Z]+(?:_[A-Z]+)*?)_LEVEL_(\\d)");

    private TradeKeys() {
    }

    public static ResourceKey<VillagerTrade> trade(String constant) {
        return ResourceKey.create(ResourceKey.createRegistryKey(Identifier.withDefaultNamespace("villager_trade")),
            Identifier.withDefaultNamespace(tradePath(constant)));
    }

    public static ResourceKey<TradeSet> set(String constant) {
        return ResourceKey.create(ResourceKey.createRegistryKey(Identifier.withDefaultNamespace("trade_set")),
            Identifier.withDefaultNamespace(setPath(constant)));
    }

    static String tradePath(String constant) {
        String lower = constant.toLowerCase(Locale.ROOT);
        if (constant.startsWith("WANDERING_TRADER_")) {
            return "wandering_trader/" + lower.substring("wandering_trader_".length());
        }
        Matcher leveled = LEVELED.matcher(constant);
        if (!leveled.matches()) {
            return lower;
        }
        String profession = leveled.group(1).toLowerCase(Locale.ROOT);
        String rest = leveled.group(3).toLowerCase(Locale.ROOT);
        if (profession.equals("common_smith")) {
            profession = "smith";
        }
        if (profession.equals("toolsmith") && rest.matches("emerald_(iron|diamond)_(axe|shovel|pickaxe)")) {
            rest = rest.replace("emerald_", "emerald_enchanted_");
        }
        if (rest.equals("emerald_and_book_enchanted_book")) {
            rest = "emerald_book_and_enchanted_book";
        }
        return profession + "/" + leveled.group(2) + "/" + rest.replace("grantite", "granite");
    }

    static String setPath(String constant) {
        Matcher leveled = SET.matcher(constant);
        if (leveled.matches()) {
            return leveled.group(1).toLowerCase(Locale.ROOT) + "/level_" + leveled.group(2);
        }
        String lower = constant.toLowerCase(Locale.ROOT);
        return lower.startsWith("wandering_trader_") ? "wandering_trader/" + lower.substring(17) : lower;
    }
}
