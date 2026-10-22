package org.bukkit.craftbukkit.util;

import net.minecraft.resources.Identifier;
import org.bukkit.NamespacedKey;

public final class CraftNamespacedKey {
    private CraftNamespacedKey() {}

    public static NamespacedKey fromMinecraft(Identifier minecraft) {
        return new NamespacedKey(minecraft.getNamespace(), minecraft.getPath());
    }

    public static NamespacedKey fromString(String string) {
        return NamespacedKey.fromString(string);
    }

    public static Identifier toMinecraft(NamespacedKey key) {
        return Identifier.fromNamespaceAndPath(key.getNamespace(), key.getKey());
    }
}
