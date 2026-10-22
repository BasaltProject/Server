package net.minecraft.core;

import net.minecraft.resources.Identifier;

public interface DefaultedRegistry<T> extends Registry<T> {
    Identifier getDefaultKey();
}
