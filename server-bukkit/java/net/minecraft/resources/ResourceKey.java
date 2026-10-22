package net.minecraft.resources;

import org.basaltmc.bukkit.Unsupported;
public class ResourceKey<T> {
    protected ResourceKey() {
    }
    public static ResourceKey create(ResourceKey registryName, Identifier location) {
        throw Unsupported.hit("net.minecraft.resources.ResourceKey.create");
    }
    public Identifier identifier() {
        throw Unsupported.hit("net.minecraft.resources.ResourceKey.identifier");
    }
}
