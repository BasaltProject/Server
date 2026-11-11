// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.data.tags;

import java.util.concurrent.CompletableFuture;
import net.minecraft.core.HolderLookup;
import net.minecraft.core.registries.Registries;
import net.minecraft.data.PackOutput;
import net.minecraft.world.item.enchantment.Enchantment;

public class TradeRebalanceEnchantmentTagsProvider extends TagsProvider<Enchantment> {
    public TradeRebalanceEnchantmentTagsProvider(PackOutput output, CompletableFuture<HolderLookup.Provider> lookupProvider) {
        super(output, (net.minecraft.resources.ResourceKey) Registries.ENCHANTMENT, lookupProvider);
    }

    @Override
    protected void addTags(HolderLookup.Provider registries) {
        addNativeTags("data/minecraft/datapacks/trade_rebalance/data/minecraft/");
    }
}
