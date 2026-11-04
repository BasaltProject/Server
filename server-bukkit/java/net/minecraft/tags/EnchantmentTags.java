// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.tags;

import net.minecraft.core.registries.Registries;
import net.minecraft.resources.Identifier;
import net.minecraft.world.item.enchantment.Enchantment;

public interface EnchantmentTags {
    TagKey<Enchantment> TOOLTIP_ORDER = org.basaltmc.bukkit.facade.Tags.key(Registries.ENCHANTMENT, "tooltip_order");
    TagKey<Enchantment> ARMOR_EXCLUSIVE = org.basaltmc.bukkit.facade.Tags.key(Registries.ENCHANTMENT, "exclusive_set/armor");
    TagKey<Enchantment> BOOTS_EXCLUSIVE = org.basaltmc.bukkit.facade.Tags.key(Registries.ENCHANTMENT, "exclusive_set/boots");
    TagKey<Enchantment> BOW_EXCLUSIVE = org.basaltmc.bukkit.facade.Tags.key(Registries.ENCHANTMENT, "exclusive_set/bow");
    TagKey<Enchantment> CROSSBOW_EXCLUSIVE = org.basaltmc.bukkit.facade.Tags.key(Registries.ENCHANTMENT, "exclusive_set/crossbow");
    TagKey<Enchantment> DAMAGE_EXCLUSIVE = org.basaltmc.bukkit.facade.Tags.key(Registries.ENCHANTMENT, "exclusive_set/damage");
    TagKey<Enchantment> MINING_EXCLUSIVE = org.basaltmc.bukkit.facade.Tags.key(Registries.ENCHANTMENT, "exclusive_set/mining");
    TagKey<Enchantment> RIPTIDE_EXCLUSIVE = org.basaltmc.bukkit.facade.Tags.key(Registries.ENCHANTMENT, "exclusive_set/riptide");
    TagKey<Enchantment> TRADEABLE = org.basaltmc.bukkit.facade.Tags.key(Registries.ENCHANTMENT, "tradeable");
    TagKey<Enchantment> DOUBLE_TRADE_PRICE = org.basaltmc.bukkit.facade.Tags.key(Registries.ENCHANTMENT, "double_trade_price");
    TagKey<Enchantment> IN_ENCHANTING_TABLE = org.basaltmc.bukkit.facade.Tags.key(Registries.ENCHANTMENT, "in_enchanting_table");
    TagKey<Enchantment> ON_MOB_SPAWN_EQUIPMENT = org.basaltmc.bukkit.facade.Tags.key(Registries.ENCHANTMENT, "on_mob_spawn_equipment");
    TagKey<Enchantment> ON_TRADED_EQUIPMENT = org.basaltmc.bukkit.facade.Tags.key(Registries.ENCHANTMENT, "on_traded_equipment");
    TagKey<Enchantment> ON_RANDOM_LOOT = org.basaltmc.bukkit.facade.Tags.key(Registries.ENCHANTMENT, "on_random_loot");
    TagKey<Enchantment> CURSE = org.basaltmc.bukkit.facade.Tags.key(Registries.ENCHANTMENT, "curse");
    TagKey<Enchantment> SMELTS_LOOT = org.basaltmc.bukkit.facade.Tags.key(Registries.ENCHANTMENT, "smelts_loot");
    TagKey<Enchantment> PREVENTS_BEE_SPAWNS_WHEN_MINING = org.basaltmc.bukkit.facade.Tags.key(Registries.ENCHANTMENT, "prevents_bee_spawns_when_mining");
    TagKey<Enchantment> PREVENTS_DECORATED_POT_SHATTERING = org.basaltmc.bukkit.facade.Tags.key(Registries.ENCHANTMENT, "prevents_decorated_pot_shattering");
    TagKey<Enchantment> PREVENTS_ICE_MELTING = org.basaltmc.bukkit.facade.Tags.key(Registries.ENCHANTMENT, "prevents_ice_melting");
    TagKey<Enchantment> PREVENTS_INFESTED_SPAWNS = org.basaltmc.bukkit.facade.Tags.key(Registries.ENCHANTMENT, "prevents_infested_spawns");
    TagKey<Enchantment> TREASURE = org.basaltmc.bukkit.facade.Tags.key(Registries.ENCHANTMENT, "treasure");
    TagKey<Enchantment> NON_TREASURE = org.basaltmc.bukkit.facade.Tags.key(Registries.ENCHANTMENT, "non_treasure");
    TagKey<Enchantment> TRADES_DESERT_COMMON = org.basaltmc.bukkit.facade.Tags.key(Registries.ENCHANTMENT, "trades/desert_common");
    TagKey<Enchantment> TRADES_JUNGLE_COMMON = org.basaltmc.bukkit.facade.Tags.key(Registries.ENCHANTMENT, "trades/jungle_common");
    TagKey<Enchantment> TRADES_PLAINS_COMMON = org.basaltmc.bukkit.facade.Tags.key(Registries.ENCHANTMENT, "trades/plains_common");
    TagKey<Enchantment> TRADES_SAVANNA_COMMON = org.basaltmc.bukkit.facade.Tags.key(Registries.ENCHANTMENT, "trades/savanna_common");
    TagKey<Enchantment> TRADES_SNOW_COMMON = org.basaltmc.bukkit.facade.Tags.key(Registries.ENCHANTMENT, "trades/snow_common");
    TagKey<Enchantment> TRADES_SWAMP_COMMON = org.basaltmc.bukkit.facade.Tags.key(Registries.ENCHANTMENT, "trades/swamp_common");
    TagKey<Enchantment> TRADES_TAIGA_COMMON = org.basaltmc.bukkit.facade.Tags.key(Registries.ENCHANTMENT, "trades/taiga_common");
}
