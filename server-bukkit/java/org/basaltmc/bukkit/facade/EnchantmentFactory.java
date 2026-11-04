package org.basaltmc.bukkit.facade;

import java.util.ArrayList;
import java.util.List;
import java.util.Optional;
import net.minecraft.core.Holder;
import net.minecraft.core.HolderSet;
import net.minecraft.core.component.DataComponentMap;
import net.minecraft.nbt.CompoundTag;
import net.minecraft.nbt.NbtOps;
import net.minecraft.nbt.StringTag;
import net.minecraft.nbt.Tag;
import net.minecraft.network.chat.Component;
import net.minecraft.resources.Identifier;
import net.minecraft.world.entity.EquipmentSlotGroup;
import net.minecraft.world.item.Item;
import net.minecraft.world.item.enchantment.Enchantment;
import org.basaltmc.bukkit.InventoryNatives;
import org.basaltmc.bukkit.item.Nbt;
import org.basaltmc.bukkit.item.TagSets;

/** Builds an enchantment value from the native enchantment definition (cost, weight, slots, supported items, exclusivity). */
public final class EnchantmentFactory {
    private EnchantmentFactory() {}

    public static Enchantment build(Identifier id) {
        byte[] bytes = InventoryNatives.enchantmentData(id.toString());
        java.util.Map<String, Object> data = bytes == null ? null : Nbt.read(bytes);
        if (data == null) {
            return null;
        }
        HolderSet<Item> supported = items(data.get("supported_items"));
        Optional<HolderSet<Item>> primary = data.containsKey("primary_items") ? Optional.of(items(data.get("primary_items"))) : Optional.empty();
        List<EquipmentSlotGroup> slots = new ArrayList<>();
        Object slotSpec = data.get("slots");
        if (slotSpec instanceof List<?> names) {
            for (Object name : names) {
                EquipmentSlotGroup.CODEC.parse(NbtOps.INSTANCE, StringTag.valueOf(String.valueOf(name))).result().ifPresent(slots::add);
            }
        }
        Enchantment.EnchantmentDefinition definition = new Enchantment.EnchantmentDefinition(supported, primary,
            Nbt.integer(data, "weight", 1), Nbt.integer(data, "max_level", 1), cost(Nbt.map(data, "min_cost")),
            cost(Nbt.map(data, "max_cost")), Nbt.integer(data, "anvil_cost", 0), slots);
        Component description = Component.translatable("enchantment." + id.getNamespace() + "." + id.getPath().replace('/', '.'));
        Enchantment made = new Enchantment(description, definition, enchantments(data.get("exclusive_set")), DataComponentMap.EMPTY);
        EnchantmentEffects.remember(made, data.get("effects"));
        return made;
    }

    private static Enchantment.Cost cost(java.util.Map<String, Object> tag) {
        return new Enchantment.Cost(Nbt.integer(tag, "base", 0), Nbt.integer(tag, "per_level_above_first", 0));
    }

    private static HolderSet<Item> items(Object spec) {
        List<Holder<Item>> holders = new ArrayList<>();
        for (String entry : TagSets.resolve("items", spec)) {
            holders.add(HolderRefs.of(Item.class, Identifier.parse(entry)));
        }
        return HolderSet.direct(holders);
    }

    private static HolderSet<Enchantment> enchantments(Object spec) {
        if (spec == null) {
            return HolderSet.empty();
        }
        List<Holder<Enchantment>> holders = new ArrayList<>();
        for (String entry : TagSets.resolve("enchantment", spec)) {
            holders.add(HolderRefs.of(Enchantment.class, Identifier.parse(entry)));
        }
        return HolderSet.direct(holders);
    }
}
