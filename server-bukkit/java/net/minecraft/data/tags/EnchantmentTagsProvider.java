// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.data.tags;

import java.util.concurrent.CompletableFuture;
import net.minecraft.core.HolderLookup;
import net.minecraft.core.registries.Registries;
import net.minecraft.data.PackOutput;
import net.minecraft.resources.ResourceKey;
import net.minecraft.tags.EnchantmentTags;
import net.minecraft.world.item.enchantment.Enchantment;

public abstract class EnchantmentTagsProvider extends TagsProvider<Enchantment> {
    public EnchantmentTagsProvider(PackOutput output, CompletableFuture<HolderLookup.Provider> lookupProvider) {
        super(output, (net.minecraft.resources.ResourceKey) Registries.ENCHANTMENT, lookupProvider);
    }

    @SafeVarargs
    protected final void tooltipOrder(HolderLookup.Provider registries, ResourceKey<Enchantment>... order) {
        tag(EnchantmentTags.TOOLTIP_ORDER).add(order);
    }
}
