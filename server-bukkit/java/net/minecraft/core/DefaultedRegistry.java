// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.core;

import net.minecraft.resources.Identifier;

public interface DefaultedRegistry<T> extends Registry<T> {
    Identifier getDefaultKey();

    @Override
    Identifier getKey(T thing);

    @Override
    T getValue(Identifier key);

    @Override
    T byId(int id);
}
