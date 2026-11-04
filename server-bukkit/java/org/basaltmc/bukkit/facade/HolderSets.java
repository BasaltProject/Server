package org.basaltmc.bukkit.facade;

import net.minecraft.core.Holder;
import net.minecraft.core.HolderSet;

/** Membership tests that resolve tag sets which were parsed but never bound to a registry. */
public final class HolderSets {
    private HolderSets() {}

    public static <T> boolean contains(HolderSet<T> set, Holder<T> holder) {
        if (set instanceof HolderSet.Named<T> named && !named.isBound()) {
            return holder.is(named.key());
        }
        return set.contains(holder);
    }
}
