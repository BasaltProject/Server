// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.core;

public interface HolderOwner<T> {
    default boolean canSerializeIn(HolderOwner<T> owner) {
        return owner == this;
    }
    default boolean canSerialize(HolderOwner<T> owner) {
        return canSerializeIn(owner);
    }
}
