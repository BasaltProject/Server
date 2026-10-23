package org.basaltmc.bukkit.item;

import java.util.Map;
import org.bukkit.Material;
import org.bukkit.configuration.serialization.DelegateDeserialization;
import org.bukkit.enchantments.Enchantment;
import org.bukkit.inventory.meta.EnchantmentStorageMeta;

@DelegateDeserialization(ItemMetas.Serial.class)
public final class MetaEnchantedBook extends MetaItem implements EnchantmentStorageMeta {
    public MetaEnchantedBook(Material type, Map<String, Object> components) {
        super(type, components);
    }

    @Override
    public MetaEnchantedBook clone() {
        return (MetaEnchantedBook) super.clone();
    }

    @Override
    public boolean hasStoredEnchants() {
        return hasEnchantsIn(kStoredEnchantments);
    }

    @Override
    public boolean hasStoredEnchant(Enchantment enchant) {
        Map<String, Object> map = compoundOf(kStoredEnchantments);
        return map != null && enchant != null && map.containsKey(enchant.getKey().toString());
    }

    @Override
    public int getStoredEnchantLevel(Enchantment enchant) {
        return Nbt.integer(compoundOf(kStoredEnchantments), enchant.getKey().toString(), 0);
    }

    @Override
    public Map<Enchantment, Integer> getStoredEnchants() {
        return enchantsIn(kStoredEnchantments);
    }

    @Override
    public boolean addStoredEnchant(Enchantment enchant, int level, boolean ignoreLevelRestriction) {
        return addEnchantTo(kStoredEnchantments, enchant, level, ignoreLevelRestriction);
    }

    @Override
    public boolean removeStoredEnchant(Enchantment enchant) {
        return enchant != null && removeEnchantFrom(kStoredEnchantments, enchant);
    }

    @Override
    public boolean hasConflictingStoredEnchant(Enchantment enchant) {
        if (enchant == null) {
            return false;
        }
        for (Enchantment existing : getStoredEnchants().keySet()) {
            if (existing.conflictsWith(enchant)) {
                return true;
            }
        }
        return false;
    }
}
