// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.server.level;

import com.mojang.serialization.Codec;
import com.mojang.serialization.MapCodec;
import com.mojang.serialization.codecs.RecordCodecBuilder;
import net.minecraft.core.registries.BuiltInRegistries;
import net.minecraft.util.ExtraCodecs;
import net.minecraft.util.Util;

public class Ticket {
    public static final MapCodec<Ticket> CODEC = RecordCodecBuilder.mapCodec(group -> group.group(
        BuiltInRegistries.TICKET_TYPE.byNameCodec().fieldOf("type").forGetter(Ticket::getType),
        ExtraCodecs.NON_NEGATIVE_INT.fieldOf("level").forGetter(Ticket::getTicketLevel),
        Codec.LONG.optionalFieldOf("ticks_left", 0L).forGetter(ticket -> ticket.ticksLeft)).apply(group, Ticket::new));
    private final TicketType type;
    private final int ticketLevel;
    private long ticksLeft;

    public Ticket(TicketType type, int ticketLevel) {
        this(type, ticketLevel, type.timeout());
    }

    private Ticket(TicketType type, int ticketLevel, long ticksLeft) {
        this.type = type;
        this.ticketLevel = ticketLevel;
        this.ticksLeft = ticksLeft;
    }

    public String toString() {
        String name = Util.getRegisteredName(BuiltInRegistries.TICKET_TYPE, type);
        if (!type.hasTimeout()) {
            return "Ticket[" + name + " " + ticketLevel + "] with no timeout";
        }
        return "Ticket[" + name + " " + ticketLevel + "] with " + ticksLeft + " ticks left ( out of" + type.timeout() + ")";
    }

    public TicketType getType() {
        return type;
    }

    public int getTicketLevel() {
        return ticketLevel;
    }

    public void resetTicksLeft() {
        ticksLeft = type.timeout();
    }

    public void decreaseTicksLeft() {
        if (type.hasTimeout()) {
            ticksLeft--;
        }
    }

    public boolean isTimedOut() {
        return type.hasTimeout() && ticksLeft < 0L;
    }
}
