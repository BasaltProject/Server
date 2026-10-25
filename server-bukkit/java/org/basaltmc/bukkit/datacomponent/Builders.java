package org.basaltmc.bukkit.datacomponent;

import io.papermc.paper.datacomponent.item.DamageResistant;
import io.papermc.paper.datacomponent.item.DyedItemColor;
import io.papermc.paper.datacomponent.item.Enchantable;
import io.papermc.paper.datacomponent.item.FoodProperties;
import io.papermc.paper.datacomponent.item.ItemEnchantments;
import io.papermc.paper.datacomponent.item.ItemLore;
import io.papermc.paper.datacomponent.item.MapId;
import io.papermc.paper.datacomponent.item.OminousBottleAmplifier;
import io.papermc.paper.datacomponent.item.Repairable;
import io.papermc.paper.datacomponent.item.TooltipDisplay;
import io.papermc.paper.datacomponent.item.UseCooldown;
import io.papermc.paper.datacomponent.item.UseRemainder;
import io.papermc.paper.registry.set.RegistryKeySet;
import org.bukkit.damage.DamageType;
import org.bukkit.inventory.ItemStack;
import org.bukkit.inventory.ItemType;

public final class Builders {
    private Builders() {}

    public static ItemLore.Builder lore() {
        return new PaperValues.LoreBuilder();
    }

    public static ItemEnchantments.Builder enchantments() {
        return new PaperValues.EnchantsBuilder();
    }

    public static FoodProperties.Builder food() {
        return new PaperValues.FoodBuilder();
    }

    public static DyedItemColor.Builder dyed() {
        return new PaperValues.DyedBuilder();
    }

    public static io.papermc.paper.datacomponent.item.CustomModelData.Builder model() {
        return new PaperValues.ModelBuilder();
    }

    public static MapId mapId(int id) {
        return new PaperValues.MapIdValue(id);
    }

    public static UseRemainder remainder(ItemStack stack) {
        return new PaperValues.RemainderValue(stack.clone());
    }

    public static UseCooldown.Builder cooldown(float seconds) {
        return new PaperValues.CooldownBuilder(seconds);
    }

    public static DamageResistant damageResistant(RegistryKeySet<DamageType> types) {
        return new PaperValues.DamageResistantValue(types);
    }

    public static Object damageResistantNbt(RegistryKeySet<DamageType> types) {
        return new PaperValues.DamageResistantValue(types).nbt();
    }

    public static DamageResistant damageResistantOf(Object stored) {
        return PaperValues.DamageResistantValue.of(stored);
    }

    public static Enchantable enchantable(int level) {
        return new PaperValues.EnchantableValue(level);
    }

    public static Repairable repairable(RegistryKeySet<ItemType> types) {
        return new PaperValues.RepairableValue(types);
    }

    public static OminousBottleAmplifier amplifier(int amplifier) {
        return new PaperValues.AmplifierValue(amplifier);
    }

    public static TooltipDisplay.Builder tooltip() {
        return new PaperValues.TooltipBuilder();
    }
}
