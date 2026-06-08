package org.bukkit.craftbukkit.util;

import net.minecraft.resources.Identifier;
import org.basaltmc.bukkit.Unsupported;
import org.bukkit.NamespacedKey;
public class CraftNamespacedKey {
    protected CraftNamespacedKey() {
    }
    public static NamespacedKey fromMinecraft(Identifier minecraft) {
        throw Unsupported.hit("org.bukkit.craftbukkit.util.CraftNamespacedKey.fromMinecraft");
    }
}
