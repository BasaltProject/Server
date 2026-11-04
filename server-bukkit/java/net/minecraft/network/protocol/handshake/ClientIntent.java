// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.network.protocol.handshake;

public enum ClientIntent {
    STATUS, LOGIN, TRANSFER;

    private static final int kFirstId = 1;

    public static ClientIntent byId(int id) {
        return switch (id) {
            case 1 -> STATUS;
            case 2 -> LOGIN;
            case 3 -> TRANSFER;
            default -> throw new IllegalArgumentException("Unknown connection intention: " + id);
        };
    }

    public int id() {
        return ordinal() + kFirstId;
    }
}
