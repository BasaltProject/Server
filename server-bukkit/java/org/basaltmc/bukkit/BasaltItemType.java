package org.basaltmc.bukkit;

import java.util.function.Consumer;
import org.bukkit.Material;
import org.bukkit.NamespacedKey;
import org.bukkit.block.BlockType;
import org.bukkit.inventory.ItemStack;
import org.bukkit.inventory.ItemType;
import org.bukkit.inventory.meta.ItemMeta;
import org.basaltmc.bukkit.gen.ItemTypeTypedBase;
import org.basaltmc.bukkit.item.ItemData;
import org.basaltmc.bukkit.item.ItemMetas;

@SuppressWarnings({"rawtypes", "unchecked"})
public final class BasaltItemType extends ItemTypeTypedBase {
    private final NamespacedKey key;

    BasaltItemType(String id) {
        this.key = NamespacedKey.fromString(id);
    }

    private Material material() {
        return Material.getMaterial(key.getKey().toUpperCase(java.util.Locale.ROOT));
    }

    @Override
    public NamespacedKey getKey() {
        return key;
    }

    @Override
    public ItemType.Typed typed() {
        return this;
    }

    @Override
    public ItemType.Typed typed(Class type) {
        return this;
    }

    @Override
    public Class getItemMetaClass() {
        return ItemMetas.metaClass(material());
    }

    @Override
    public ItemStack createItemStack() {
        return new BasaltItemStack(material(), 1);
    }

    @Override
    public ItemStack createItemStack(int amount) {
        return new BasaltItemStack(material(), amount);
    }

    @Override
    public ItemStack createItemStack(Consumer consumer) {
        return createItemStack(1, consumer);
    }

    @Override
    public ItemStack createItemStack(int amount, Consumer consumer) {
        ItemStack stack = createItemStack(amount);
        ItemMeta meta = stack.getItemMeta();
        consumer.accept(meta);
        stack.setItemMeta(meta);
        return stack;
    }

    @Override
    public boolean hasBlockType() {
        return Registries.blockIndex(key.toString()) >= 0;
    }

    @Override
    public BlockType getBlockType() {
        int block = Registries.blockIndex(key.toString());
        return block < 0 ? null : BasaltRegistryAccess.blockType(block);
    }

    @Override
    public int getMaxStackSize() {
        return ItemData.of(key.toString()).maxStackSize();
    }

    @Override
    public short getMaxDurability() {
        return (short) ItemData.of(key.toString()).maxDamage();
    }

    @Override
    public boolean isEdible() {
        return ItemData.of(key.toString()).food() != null;
    }

    @Override
    public boolean isRecord() {
        return key.getKey().startsWith("music_disc_");
    }

    @Override
    public boolean isFuel() {
        return ItemData.of(key.toString()).burnTicks() > 0;
    }

    @Override
    public int getBurnDuration() {
        return ItemData.of(key.toString()).burnTicks();
    }

    @Override
    public ItemType getCraftingRemainingItem() {
        String remainder = ItemData.of(key.toString()).craftingRemainder();
        return remainder == null ? null : org.bukkit.Registry.ITEM.get(NamespacedKey.fromString(remainder));
    }

    @Override
    public boolean isCompostable() {
        return false;
    }

    @Override
    public Material asMaterial() {
        return material();
    }

    @Override
    public String getTranslationKey() {
        return "item." + key.getNamespace() + "." + key.getKey();
    }

    @Override
    public String translationKey() {
        return getTranslationKey();
    }

    @Override
    public boolean isEnabledByFeature(org.bukkit.World world) {
        return true;
    }

    @Override
    public boolean equals(Object other) {
        return other instanceof BasaltItemType type && type.key.equals(key);
    }

    @Override
    public int hashCode() {
        return key.hashCode();
    }

    @Override
    public String toString() {
        return key.toString();
    }
}
