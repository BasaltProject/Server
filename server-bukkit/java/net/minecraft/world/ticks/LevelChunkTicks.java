// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.ticks;
import it.unimi.dsi.fastutil.objects.ObjectOpenCustomHashSet;
import java.util.ArrayList;
import java.util.Comparator;
import java.util.Iterator;
import java.util.List;
import java.util.PriorityQueue;
import java.util.Queue;
import java.util.Set;
import java.util.function.BiConsumer;
import java.util.function.Predicate;
import java.util.stream.Stream;
import net.minecraft.core.BlockPos;
public class LevelChunkTicks<T> implements TickContainerAccess<T>, SerializableTickContainer<T> {
    private static final Comparator<ScheduledTick<?>> kBySubTick = Comparator.comparingLong(ScheduledTick::subTickOrder);
    private final Queue<ScheduledTick<T>> queue = new PriorityQueue<>(ScheduledTick.DRAIN_ORDER);
    private final Set<ScheduledTick<?>> occupied = new ObjectOpenCustomHashSet<>(ScheduledTick.UNIQUE_TICK_HASH);
    private List<SavedTick<T>> pending;
    private BiConsumer<LevelChunkTicks<T>, ScheduledTick<T>> listener;
    public LevelChunkTicks() {
    }
    public LevelChunkTicks(List<SavedTick<T>> pendingTicks) {
        this.pending = pendingTicks;
        for (SavedTick<T> saved : pendingTicks) {
            occupied.add(ScheduledTick.probe(saved.type(), saved.pos()));
        }
    }
    public void setOnTickAdded(BiConsumer<LevelChunkTicks<T>, ScheduledTick<T>> onTickAdded) {
        this.listener = onTickAdded;
    }
    public ScheduledTick<T> peek() {
        return queue.peek();
    }
    public ScheduledTick<T> poll() {
        ScheduledTick<T> head = queue.poll();
        if (head != null) {
            occupied.remove(head);
        }
        return head;
    }
    public void schedule(ScheduledTick<T> tick) {
        if (occupied.add(tick)) {
            enqueue(tick);
        }
    }
    private void enqueue(ScheduledTick<T> tick) {
        queue.add(tick);
        if (listener != null) {
            listener.accept(this, tick);
        }
    }
    public boolean hasScheduledTick(BlockPos pos, T type) {
        return occupied.contains(ScheduledTick.probe(type, pos));
    }
    public void removeIf(Predicate<ScheduledTick<T>> test) {
        Iterator<ScheduledTick<T>> it = queue.iterator();
        while (it.hasNext()) {
            ScheduledTick<T> tick = it.next();
            if (test.test(tick)) {
                it.remove();
                occupied.remove(tick);
            }
        }
    }
    public Stream<ScheduledTick<T>> getAll() {
        return queue.stream();
    }
    public int count() {
        return queue.size() + (pending != null ? pending.size() : 0);
    }
    public List<SavedTick<T>> pack(long currentTick) {
        List<SavedTick<T>> out = new ArrayList<>(queue.size());
        if (pending != null) {
            out.addAll(pending);
        }
        List<ScheduledTick<T>> ordered = new ArrayList<>(queue);
        ordered.sort(kBySubTick);
        for (ScheduledTick<T> tick : ordered) {
            out.add(tick.toSavedTick(currentTick));
        }
        return out;
    }
    public void unpack(long currentTick) {
        if (pending != null) {
            int base = -pending.size();
            for (SavedTick<T> saved : pending) {
                enqueue(saved.unpack(currentTick, base++));
            }
        }
        pending = null;
    }
}
