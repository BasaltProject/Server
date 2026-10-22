package net.minecraft.server.level;
public record TicketType(long timeout, Object flags) {
    public TicketType(long value, int value2) {
        this(0L, null);
    }
}
