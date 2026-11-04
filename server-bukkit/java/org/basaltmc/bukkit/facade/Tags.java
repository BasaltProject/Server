package org.basaltmc.bukkit.facade;

import net.minecraft.resources.Identifier;
import net.minecraft.resources.ResourceKey;
import net.minecraft.tags.TagKey;

public final class Tags {
    private Tags() {}

    @SuppressWarnings({"unchecked", "rawtypes"})
    public static <T> TagKey<T> key(ResourceKey<?> registry, String name) {
        return TagKey.create((ResourceKey) registry, Identifier.withDefaultNamespace(name));
    }
}
