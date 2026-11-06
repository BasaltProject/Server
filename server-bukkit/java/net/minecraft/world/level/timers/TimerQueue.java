// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.timers;

import com.google.common.primitives.UnsignedLong;
import com.mojang.serialization.Codec;
import java.util.HashSet;
import java.util.List;
import java.util.PriorityQueue;
import java.util.Set;
import net.minecraft.server.MinecraftServer;
import net.minecraft.world.level.saveddata.SavedData;
import net.minecraft.world.level.saveddata.SavedDataType;

public class TimerQueue<T> extends SavedData {
    public static final Codec<TimerQueue<MinecraftServer>> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.codec(net.minecraft.world.level.timers.TimerQueue.class, "CODEC");
    public static final SavedDataType<TimerQueue<MinecraftServer>> TYPE = null;
    private final PriorityQueue<Event<T>> queue = new PriorityQueue<>((a, b) -> {
        int byTime = Long.compare(a.triggerTime(), b.triggerTime());
        return byTime != 0 ? byTime : a.sequentialId().compareTo(b.sequentialId());
    });
    private final Set<String> ids = new HashSet<>();
    private UnsignedLong sequence = UnsignedLong.ZERO;

    protected static <T> Codec<TimerQueue<T>> codec(TimerCallbacks<T> callbacks) {
        return com.mojang.serialization.MapCodec.unit(() -> new TimerQueue<T>()).codec();
    }

    public TimerQueue(Packed<T> packedEvents) {
        packedEvents.events().forEach(event -> schedule(event.id(), event.triggerTime(), event.callback()));
    }

    public TimerQueue() {}

    public void tick(T context, long currentTick) {
        while (true) {
            Event<T> next = queue.peek();
            if (next == null || next.triggerTime() > currentTick) {
                return;
            }
            queue.remove();
            ids.remove(next.id());
            next.callback().handle(context, this, currentTick);
        }
    }

    public void schedule(String id, long time, TimerCallback<T> callback) {
        if (ids.add(id)) {
            sequence = sequence.plus(UnsignedLong.ONE);
            queue.add(new Event<>(time, sequence, id, callback));
            setDirty();
        }
    }

    public int remove(String id) {
        boolean removed = queue.removeIf(event -> event.id().equals(id));
        ids.remove(id);
        if (removed) {
            setDirty();
        }
        return removed ? 1 : 0;
    }

    public Set<String> getEventsIds() {
        return Set.copyOf(ids);
    }

    protected Packed<T> pack() {
        return new Packed<>(queue.stream().map(event -> new Event.Packed<>(event.triggerTime(), event.id(), event.callback())).toList());
    }

    public static record Event<T>(long triggerTime, UnsignedLong sequentialId, String id, TimerCallback<T> callback) {
        public static record Packed<T>(long triggerTime, String id, TimerCallback<T> callback) {
            public static <T> Codec<Packed<T>> codec(Codec<TimerCallback<T>> callbackCodec) {
                return com.mojang.serialization.codecs.RecordCodecBuilder.create(i -> i.group(
                    Codec.LONG.fieldOf("trigger_time").forGetter(Packed::triggerTime),
                    Codec.STRING.fieldOf("id").forGetter(Packed::id),
                    callbackCodec.fieldOf("callback").forGetter(Packed::callback)).apply(i, Packed::new));
            }
        }
    }

    public static record Packed<T>(List<Event.Packed<T>> events) {
        public static <T> Codec<Packed<T>> codec(Codec<TimerCallback<T>> callbackCodec) {
            return Event.Packed.codec(callbackCodec).listOf().xmap(Packed::new, Packed::events);
        }
    }
}
