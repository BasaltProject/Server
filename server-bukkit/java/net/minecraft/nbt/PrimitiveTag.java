// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.nbt;
public interface PrimitiveTag extends Tag {
    @Override
    default Tag copy() {
        return this;
    }
}
