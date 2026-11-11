// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.data.registries;

import net.minecraft.core.HolderLookup;

public class VanillaRegistries {
    public static void validateThatAllBiomeFeaturesHaveBiomeFilter(HolderLookup.Provider registries) {
    }

    public static void validateLootData(HolderLookup.Provider newRegistries) {
    }

    public static HolderLookup.Provider createWorldLookup() {
        return HolderLookup.Provider.create(java.util.stream.Stream.empty());
    }

    public static HolderLookup.Provider createReloadableLookup(HolderLookup.Provider context) {
        return HolderLookup.Provider.create(java.util.stream.Stream.empty());
    }
}
