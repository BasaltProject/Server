// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.server.level;
public record TicketType(long timeout, int flags) {    public static final long NO_TIMEOUT = 0L;
    public static final int FLAG_PERSIST = 1;
    public static final int FLAG_LOADING = 2;
    public static final int FLAG_SIMULATION = 4;
    public static final int FLAG_KEEP_DIMENSION_ACTIVE = 8;
    public static final int FLAG_CAN_EXPIRE_IF_UNLOADED = 16;
    public static final TicketType PLAYER_SPAWN = new TicketType(20L, 2);
    public static final TicketType SPAWN_SEARCH = new TicketType(1L, 2);
    public static final TicketType DRAGON = new TicketType(0L, 6);
    public static final TicketType PLAYER_LOADING = new TicketType(0L, 2);
    public static final TicketType PLAYER_SIMULATION = new TicketType(0L, 12);
    public static final TicketType FORCED = new TicketType(0L, 15);
    public static final TicketType PORTAL = new TicketType(300L, 15);
    public static final TicketType ENDER_PEARL = new TicketType(40L, 14);
    public static final TicketType UNKNOWN = new TicketType(1L, 18);
    public boolean persist() {
        return (flags & FLAG_PERSIST) != 0;
    }
    public boolean doesLoad() {
        return (flags & FLAG_LOADING) != 0;
    }
    public boolean doesSimulate() {
        return (flags & FLAG_SIMULATION) != 0;
    }
    public boolean shouldKeepDimensionActive() {
        return (flags & FLAG_KEEP_DIMENSION_ACTIVE) != 0;
    }
    public boolean canExpireIfUnloaded() {
        return (flags & FLAG_CAN_EXPIRE_IF_UNLOADED) != 0;
    }
    public boolean hasTimeout() {
        return timeout != 0L;
    }
    public @interface Flags {
    }
}
