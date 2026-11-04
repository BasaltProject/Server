// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity;

public enum MoverType {
    SELF, PLAYER, PISTON, SHULKER_BOX, SHULKER;

    public boolean isServerAndClientSimulated() {
        return this != SELF;
    }
}
