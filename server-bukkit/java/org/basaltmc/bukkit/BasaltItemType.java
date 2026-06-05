package org.basaltmc.bukkit;

import java.util.function.Consumer;
import org.bukkit.Material;
import org.bukkit.NamespacedKey;
import org.bukkit.block.BlockType;
import org.bukkit.inventory.ItemStack;
import org.bukkit.inventory.ItemType;
import org.bukkit.inventory.meta.ItemMeta;
import org.basaltmc.bukkit.gen.ItemTypeTypedBase;

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
        return ItemMeta.class;
    }

    @Override
    public ItemStack createItemStack() {
        return new ItemStack(material(), 1);
    }

    @Override
    public ItemStack createItemStack(int amount) {
        return new ItemStack(material(), amount);
    }

    @Override
    public ItemStack createItemStack(Consumer consumer) {
        return createItemStack();
    }

    @Override
    public ItemStack createItemStack(int amount, Consumer consumer) {
        return createItemStack(amount);
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
        return 64;
    }

    @Override
    public short getMaxDurability() {
        return 0;
    }

    @Override
    public boolean isEdible() {
        return false;
    }

    @Override
    public boolean isRecord() {
        return key.getKey().startsWith("music_disc_");
    }

    @Override
    public boolean isFuel() {
        return false;
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
