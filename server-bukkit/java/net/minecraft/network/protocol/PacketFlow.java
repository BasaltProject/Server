// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.network.protocol;
public enum PacketFlow {
    SERVERBOUND("serverbound"), CLIENTBOUND("clientbound");
    private final String id;
    private PacketFlow(String id) {
        this.id = id;
    }
    public PacketFlow getOpposite() {
        return this == CLIENTBOUND ? SERVERBOUND : CLIENTBOUND;
    }
    public String id() {
        return id;
    }
}
