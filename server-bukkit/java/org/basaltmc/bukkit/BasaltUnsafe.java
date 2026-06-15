package org.basaltmc.bukkit;

import org.basaltmc.bukkit.gen.UnsafeValuesBase;

public final class BasaltUnsafe extends UnsafeValuesBase {
    static final BasaltUnsafe INSTANCE = new BasaltUnsafe();

    private static final int kDataVersion = 5023;

    @Override
    public int getDataVersion() {
        return kDataVersion;
    }

    @Override
    public boolean isSupportedApiVersion(String apiVersion) {
        return true;
    }

    @Override
    public String getMainLevelName() {
        return "world";
    }

    @Override
    public org.bukkit.potion.PotionType.InternalPotionData getInternalPotionData(org.bukkit.NamespacedKey key) {
        return org.basaltmc.bukkit.item.PotionData.of(key);
    }

    @Override
    public org.bukkit.Material getMaterial(String material, int version) {
        return org.bukkit.Material.matchMaterial(material);
    }

    @Override
    public org.bukkit.Material fromLegacy(org.bukkit.Material material) {
        return material;
    }

    @Override
    public org.bukkit.Material toLegacy(org.bukkit.Material material) {
        return material;
    }
}
