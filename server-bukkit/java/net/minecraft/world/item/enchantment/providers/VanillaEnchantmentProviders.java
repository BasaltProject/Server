// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.item.enchantment.providers;
import net.minecraft.core.HolderGetter;
import net.minecraft.core.registries.Registries;
import net.minecraft.resources.Identifier;
import net.minecraft.tags.EnchantmentTags;
import net.minecraft.util.valueproviders.ConstantInt;
import net.minecraft.world.item.enchantment.Enchantment;
import net.minecraft.world.item.enchantment.Enchantments;
import net.minecraft.data.worldgen.BootstrapContext;
import net.minecraft.resources.ResourceKey;
public interface VanillaEnchantmentProviders {
    ResourceKey<EnchantmentProvider> MOB_SPAWN_EQUIPMENT =
        create("mob_spawn_equipment");
    ResourceKey<EnchantmentProvider> RAID_PILLAGER_POST_WAVE_3 =
        create("raid/pillager_post_wave_3");
    ResourceKey<EnchantmentProvider> RAID_VINDICATOR_POST_WAVE_5 =
        create("raid/vindicator_post_wave_5");
    static void bootstrap(BootstrapContext<EnchantmentProvider> context) {
        HolderGetter<Enchantment> pool = (HolderGetter<Enchantment>) (HolderGetter) context.lookup((net.minecraft.resources.ResourceKey) Registries.ENCHANTMENT);
        context.register(MOB_SPAWN_EQUIPMENT, new EnchantmentsByCostWithDifficulty(pool.getOrThrow(EnchantmentTags.ON_MOB_SPAWN_EQUIPMENT), 5, 17));
        Object[][] singles = {
            {PILLAGER_SPAWN_CROSSBOW, Enchantments.PIERCING, 1}, {RAID_PILLAGER_POST_WAVE_3, Enchantments.QUICK_CHARGE, 1},
            {RAID_PILLAGER_POST_WAVE_5, Enchantments.QUICK_CHARGE, 2}, {RAID_VINDICATOR, Enchantments.SHARPNESS, 1},
            {RAID_VINDICATOR_POST_WAVE_5, Enchantments.SHARPNESS, 2}, {ENDERMAN_LOOT_DROP, Enchantments.SILK_TOUCH, 1}};
        for (Object[] row : singles) {
            context.register((ResourceKey<EnchantmentProvider>) row[0],
                new SingleEnchantment(pool.getOrThrow((ResourceKey<Enchantment>) row[1]), ConstantInt.of((Integer) row[2])));
        }
    }
    ResourceKey<EnchantmentProvider> RAID_VINDICATOR =
        create("raid/vindicator");
    ResourceKey<EnchantmentProvider> RAID_PILLAGER_POST_WAVE_5 =
        create("raid/pillager_post_wave_5");
    ResourceKey<EnchantmentProvider> ENDERMAN_LOOT_DROP =
        create("enderman_loot_drop");
    ResourceKey<EnchantmentProvider> PILLAGER_SPAWN_CROSSBOW =
        create("pillager_spawn_crossbow");
    static ResourceKey<EnchantmentProvider> create(String id) {
        return ResourceKey.create(Registries.ENCHANTMENT_PROVIDER, Identifier.withDefaultNamespace(id));
    }
}
