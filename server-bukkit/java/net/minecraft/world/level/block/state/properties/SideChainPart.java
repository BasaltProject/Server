// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.block.state.properties;

import net.minecraft.util.StringRepresentable;

public enum SideChainPart implements StringRepresentable {
    UNCONNECTED, RIGHT, CENTER, LEFT;

    public String toString() {
        return getSerializedName();
    }

    public String getSerializedName() {
        return name().toLowerCase(java.util.Locale.ROOT);
    }

    public boolean isConnected() {
        return this != UNCONNECTED;
    }

    public boolean isConnectionTowards(SideChainPart endPart) {
        return this == CENTER || this == endPart;
    }

    public boolean isChainEnd() {
        return this != CENTER;
    }

    public SideChainPart whenConnectedToTheRight() {
        return this == UNCONNECTED || this == LEFT ? LEFT : CENTER;
    }

    public SideChainPart whenConnectedToTheLeft() {
        return this == UNCONNECTED || this == RIGHT ? RIGHT : CENTER;
    }

    public SideChainPart whenDisconnectedFromTheRight() {
        return this == UNCONNECTED || this == LEFT ? UNCONNECTED : RIGHT;
    }

    public SideChainPart whenDisconnectedFromTheLeft() {
        return this == UNCONNECTED || this == RIGHT ? UNCONNECTED : LEFT;
    }
}
