package net.minecraft.tags;

import net.minecraft.resources.Identifier;
import net.minecraft.resources.ResourceKey;
public record TagKey(ResourceKey registry, Identifier location) {
}
