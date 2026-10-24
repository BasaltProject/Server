package org.basaltmc.bukkit;

import java.io.ByteArrayInputStream;
import java.io.ByteArrayOutputStream;
import java.io.IOException;
import java.util.LinkedHashMap;
import java.util.Map;
import java.util.zip.GZIPInputStream;
import java.util.zip.GZIPOutputStream;
import net.kyori.adventure.text.Component;
import org.basaltmc.bukkit.item.ItemData;
import org.basaltmc.bukkit.item.ItemMetas;
import org.basaltmc.bukkit.item.ItemPdc;
import org.basaltmc.bukkit.item.MetaItem;
import org.basaltmc.bukkit.item.Nbt;
import org.bukkit.Material;
import org.bukkit.configuration.serialization.DelegateDeserialization;
import org.bukkit.enchantments.Enchantment;
import org.bukkit.inventory.ItemStack;
import org.bukkit.inventory.meta.ItemMeta;
import org.bukkit.material.MaterialData;
import org.bukkit.persistence.PersistentDataContainer;

/** The server's stack: a material, a count and the vanilla data components, which every other view reads and writes. */
@DelegateDeserialization(ItemStack.class)
public final class BasaltItemStack extends ItemStack {
    private Material type;
    private int amount;
    private Map<String, Object> components;
    private Binding binding;

    /** Where a stack taken out of an inventory writes itself back, so changing it changes the slot it came from. */
    @FunctionalInterface
    public interface Binding {
        void store(ItemStack stack);
    }

    BasaltItemStack(Material type, int amount) {
        this(type, amount, Nbt.compound());
    }

    public BasaltItemStack(Material type, int amount, Map<String, Object> components) {
        super();
        this.type = type;
        this.amount = amount;
        this.components = components;
    }

    public Map<String, Object> components() {
        return components;
    }

    public BasaltItemStack bound(Binding to) {
        this.binding = to;
        return this;
    }

    public void changed() {
        if (binding != null) {
            binding.store(this);
        }
    }

    @Override
    public Material getType() {
        return type;
    }

    @Override
    public void setType(Material type) {
        this.type = type;
        changed();
    }

    @Override
    public ItemStack withType(Material type) {
        return new BasaltItemStack(type, amount, Nbt.deepCopy(components));
    }

    @Override
    public int getAmount() {
        return amount;
    }

    @Override
    public void setAmount(int amount) {
        this.amount = amount;
        changed();
    }

    @Override
    public MaterialData getData() {
        return new MaterialData(type);
    }

    @Override
    public void setData(MaterialData data) {}

    @Override
    public void setDurability(short durability) {
        if (durability <= 0) {
            components.remove("minecraft:damage");
        } else {
            components.put("minecraft:damage", (int) durability);
        }
        changed();
    }

    @Override
    public short getDurability() {
        return (short) Nbt.integer(components, "minecraft:damage", 0);
    }

    @Override
    public int getMaxStackSize() {
        return components.get("minecraft:max_stack_size") instanceof Number max ? max.intValue()
            : ItemData.of(type.getKey().toString()).maxStackSize();
    }

    @Override
    public boolean isEmpty() {
        return type.isAir() || amount <= 0;
    }

    @Override
    public boolean isSimilar(ItemStack other) {
        if (other == null) {
            return false;
        }
        if (other == this) {
            return true;
        }
        return other.getType() == type && Nbt.same(components, Items.componentsOf(other));
    }

    @Override
    public ItemStack clone() {
        return new BasaltItemStack(type, amount, Nbt.deepCopy(components));
    }

    @Override
    public boolean equals(Object other) {
        return other instanceof ItemStack stack && stack.getAmount() == amount && isSimilar(stack);
    }

    @Override
    public int hashCode() {
        return (type.hashCode() * 31 + amount) * 31 + Nbt.hash(components);
    }

    @Override
    public String toString() {
        return "ItemStack{" + type + " x " + amount + (components.isEmpty() ? "" : ", " + ItemMetas.snbt(components)) + "}";
    }

    @Override
    public boolean containsEnchantment(Enchantment enchantment) {
        return enchantment != null && getEnchantmentLevel(enchantment) > 0;
    }

    @Override
    public int getEnchantmentLevel(Enchantment enchantment) {
        return enchantment == null ? 0 : Nbt.integer(Nbt.map(components, "minecraft:enchantments"),
            enchantment.getKey().toString(), 0);
    }

    @Override
    public Map<Enchantment, Integer> getEnchantments() {
        return getItemMeta().getEnchants();
    }

    @Override
    public void addUnsafeEnchantment(Enchantment enchantment, int level) {
        if (enchantment == null) {
            throw new IllegalArgumentException("Enchantment cannot be null");
        }
        Nbt.mapOrCreate(components, "minecraft:enchantments").put(enchantment.getKey().toString(), level);
        changed();
    }

    @Override
    public int removeEnchantment(Enchantment enchantment) {
        int level = getEnchantmentLevel(enchantment);
        if (level > 0) {
            Map<String, Object> map = Nbt.map(components, "minecraft:enchantments");
            map.remove(enchantment.getKey().toString());
            if (map.isEmpty()) {
                components.remove("minecraft:enchantments");
            }
            changed();
        }
        return level;
    }

    @Override
    public void removeEnchantments() {
        components.remove("minecraft:enchantments");
        changed();
    }

    @Override
    public ItemMeta getItemMeta() {
        return ItemMetas.create(type, Nbt.deepCopy(components));
    }

    @Override
    public boolean hasItemMeta() {
        return !components.isEmpty();
    }

    @Override
    public boolean setItemMeta(ItemMeta meta) {
        if (meta == null) {
            components = Nbt.compound();
            changed();
            return true;
        }
        if (!(meta instanceof MetaItem own) || !own.applicableTo(type)) {
            return false;
        }
        components = Nbt.deepCopy(own.components());
        changed();
        return true;
    }

    @Override
    public PersistentDataContainer getPersistentDataContainer() {
        return new ItemPdc(components, this::changed);
    }

    @Override
    public boolean editPersistentDataContainer(java.util.function.Consumer<PersistentDataContainer> consumer) {
        consumer.accept(new ItemPdc(components));
        changed();
        return true;
    }

    @Override
    public ItemStack asOne() {
        return new BasaltItemStack(type, 1, Nbt.deepCopy(components));
    }

    @Override
    public ItemStack asQuantity(int quantity) {
        return new BasaltItemStack(type, quantity, Nbt.deepCopy(components));
    }

    @Override
    public ItemStack add(int quantity) {
        amount += quantity;
        changed();
        return this;
    }

    @Override
    public ItemStack subtract(int quantity) {
        amount -= quantity;
        changed();
        return this;
    }

    @Override
    public String getTranslationKey() {
        return type.translationKey();
    }

    @Override
    public String translationKey() {
        return type.translationKey();
    }

    @Override
    public Component effectiveName() {
        if (components.containsKey("minecraft:custom_name")) {
            return org.basaltmc.bukkit.item.Texts.fromNbt(components.get("minecraft:custom_name"));
        }
        if (components.containsKey("minecraft:item_name")) {
            return org.basaltmc.bukkit.item.Texts.fromNbt(components.get("minecraft:item_name"));
        }
        return Component.translatable(type.translationKey());
    }

    @Override
    public int getMaxItemUseDuration(org.bukkit.entity.LivingEntity entity) {
        Map<String, Object> food = ItemData.of(type.getKey().toString()).food();
        return food == null ? 0 : (int) (Nbt.real(food, "consume_seconds", 0) * 20);
    }

    @Override
    public Map<String, Object> serialize() {
        Map<String, Object> out = new LinkedHashMap<>();
        out.put("v", BasaltUnsafe.INSTANCE.getDataVersion());
        out.put("type", type.name());
        if (amount != 1) {
            out.put("amount", amount);
        }
        if (hasItemMeta()) {
            out.put("meta", getItemMeta());
        }
        return out;
    }

    @Override
    public byte[] serializeAsBytes() {
        Map<String, Object> root = Nbt.compound();
        root.put("DataVersion", BasaltUnsafe.INSTANCE.getDataVersion());
        root.put("id", type.getKey().toString());
        root.put("count", amount);
        root.put("components", Nbt.deepCopy(components));
        try {
            ByteArrayOutputStream bytes = new ByteArrayOutputStream();
            try (GZIPOutputStream zip = new GZIPOutputStream(bytes)) {
                zip.write(Nbt.write(root));
            }
            return bytes.toByteArray();
        } catch (IOException e) {
            throw new IllegalStateException(e);
        }
    }

    static ItemStack restore(byte[] data) {
        try (GZIPInputStream zip = new GZIPInputStream(new ByteArrayInputStream(data))) {
            Map<String, Object> root = Nbt.read(zip.readAllBytes());
            if (root == null) {
                throw new IllegalArgumentException("Not an item");
            }
            ItemStack stack = Items.fromBytes(Nbt.write(root));
            return stack == null ? ItemStack.empty() : stack;
        } catch (IOException e) {
            throw new IllegalArgumentException("Not an item", e);
        }
    }

    @Override
    public <T> T getData(io.papermc.paper.datacomponent.DataComponentType.Valued<T> type) {
        return org.basaltmc.bukkit.datacomponent.ComponentOps.get(this, type);
    }

    @Override
    public <T> T getDataOrDefault(io.papermc.paper.datacomponent.DataComponentType.Valued<? extends T> type, T fallback) {
        T found = org.basaltmc.bukkit.datacomponent.ComponentOps.get(this, type);
        return found != null ? found : fallback;
    }

    @Override
    public boolean hasData(io.papermc.paper.datacomponent.DataComponentType type) {
        return org.basaltmc.bukkit.datacomponent.ComponentOps.has(this, type);
    }

    @Override
    public java.util.Set<io.papermc.paper.datacomponent.DataComponentType> getDataTypes() {
        return org.basaltmc.bukkit.datacomponent.ComponentOps.types(this);
    }

    @Override
    public <T> void setData(io.papermc.paper.datacomponent.DataComponentType.Valued<T> type, T value) {
        org.basaltmc.bukkit.datacomponent.ComponentOps.set(this, type, value);
    }

    @Override
    public <T> void setData(io.papermc.paper.datacomponent.DataComponentType.Valued<T> type,
            io.papermc.paper.datacomponent.DataComponentBuilder<T> builder) {
        org.basaltmc.bukkit.datacomponent.ComponentOps.set(this, type, builder);
    }

    @Override
    public void setData(io.papermc.paper.datacomponent.DataComponentType.NonValued type) {
        org.basaltmc.bukkit.datacomponent.ComponentOps.set(this, type);
    }

    @Override
    public void unsetData(io.papermc.paper.datacomponent.DataComponentType type) {
        org.basaltmc.bukkit.datacomponent.ComponentOps.unset(this, type);
    }

    @Override
    public void resetData(io.papermc.paper.datacomponent.DataComponentType type) {
        org.basaltmc.bukkit.datacomponent.ComponentOps.reset(this, type);
    }

    @Override
    public boolean isDataOverridden(io.papermc.paper.datacomponent.DataComponentType type) {
        return org.basaltmc.bukkit.datacomponent.ComponentOps.overridden(this, type);
    }

    @Override
    public void copyDataFrom(ItemStack source,
            java.util.function.Predicate<io.papermc.paper.datacomponent.DataComponentType> filter) {
        org.basaltmc.bukkit.datacomponent.ComponentOps.copy(this, source, filter);
    }

    @Override
    public boolean matchesWithoutData(ItemStack item,
            java.util.Set<io.papermc.paper.datacomponent.DataComponentType> exclude, boolean ignoreCount) {
        return org.basaltmc.bukkit.datacomponent.ComponentOps.matchesWithoutData(this, item, exclude, ignoreCount);
    }

    @Override
    public java.util.List<Component> computeTooltipLines(io.papermc.paper.inventory.tooltip.TooltipContext context,
            org.bukkit.entity.Player player) {
        return org.basaltmc.bukkit.item.Tooltips.lines(this, components, effectiveName(), context);
    }

    @Override
    public boolean isRepairableBy(ItemStack repairMaterial) {
        Map<String, Object> repairable = Nbt.map(components.get("minecraft:repairable"));
        Object types = repairable == null ? components.get("minecraft:repairable") : repairable.get("items");
        if (types == null) {
            return false;
        }
        return org.basaltmc.bukkit.item.TagSets.resolve("items", types)
            .contains(repairMaterial.getType().getKey().toString());
    }
}
