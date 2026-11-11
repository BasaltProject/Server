// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.ticks;
import it.unimi.dsi.fastutil.objects.ObjectOpenCustomHashSet;
import java.util.ArrayList;
import java.util.List;
import java.util.Set;
import net.minecraft.core.BlockPos;
public class ProtoChunkTicks<T> implements TickContainerAccess<T>, SerializableTickContainer<T> {
    private final List<SavedTick<T>> queued = new ArrayList<>();
    private final Set<SavedTick<?>> seen = new ObjectOpenCustomHashSet<>(SavedTick.UNIQUE_TICK_HASH);
    public ProtoChunkTicks() {
    }
    public void schedule(ScheduledTick<T> tick) {
        add(new SavedTick<>(tick.type(), tick.pos(), 0, tick.priority()));
    }
    private void add(SavedTick<T> tick) {
        if (seen.add(tick)) {
            queued.add(tick);
        }
    }
    public boolean hasScheduledTick(BlockPos pos, T type) {
        return seen.contains(SavedTick.probe(type, pos));
    }
    public int count() {
        return queued.size();
    }
    public List<SavedTick<T>> pack(long currentTick) {
        return queued;
    }
    public List<SavedTick<T>> scheduledTicks() {
        return List.copyOf(queued);
    }
    public static <T> ProtoChunkTicks<T> load(List<SavedTick<T>> ticks) {
        ProtoChunkTicks<T> loaded = new ProtoChunkTicks<>();
        ticks.forEach(loaded::add);
        return loaded;
    }
}
