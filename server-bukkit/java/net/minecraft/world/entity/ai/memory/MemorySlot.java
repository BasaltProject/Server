// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.ai.memory;

import net.minecraft.world.entity.ai.Brain;

public class MemorySlot<T> {
    private static final long kNeverExpires = Long.MAX_VALUE;
    private T value;
    private long timeToLive;

    private MemorySlot(T value, long timeToLive) {
        this.value = value;
        this.timeToLive = timeToLive;
    }

    public void tick() {
        if (hasValue() && canExpire()) {
            if (hasExpired()) {
                clear();
            } else {
                timeToLive--;
            }
        }
    }

    public static <T> MemorySlot<T> create() {
        return new MemorySlot<>(null, kNeverExpires);
    }

    public void set(T value, long timeToLive) {
        this.value = value;
        this.timeToLive = timeToLive;
    }

    public void set(T value) {
        set(value, kNeverExpires);
    }

    public void clear() {
        value = null;
        timeToLive = kNeverExpires;
    }

    public boolean hasValue() {
        return value != null;
    }

    public T value() {
        return value;
    }

    public boolean canExpire() {
        return timeToLive != kNeverExpires;
    }

    public boolean hasExpired() {
        return timeToLive <= 0L;
    }

    public long timeToLive() {
        return timeToLive;
    }

    @Override
    public String toString() {
        if (value == null) {
            return "<empty>";
        }
        return value + (canExpire() ? " (ttl: " + timeToLive + ")" : "");
    }

    public void visit(MemoryModuleType<T> type, Brain.Visitor visitor) {
        if (value == null) {
            visitor.acceptEmpty(type);
        } else if (canExpire()) {
            visitor.accept(type, value, timeToLive);
        } else {
            visitor.accept(type, value);
        }
    }
}
