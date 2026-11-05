// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.gameevent;

import java.util.ArrayList;
import java.util.List;
import java.util.Optional;
import net.minecraft.core.Holder;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.world.phys.Vec3;

/** The listeners of one chunk section; an event reaches those whose radius covers it. */
public class SectionListeners implements GameEventListenerRegistry {
    private final ServerLevel level;
    private final List<GameEventListener> listeners = new ArrayList<>();

    public SectionListeners(ServerLevel level) {
        this.level = level;
    }

    @Override
    public boolean isEmpty() {
        return listeners.isEmpty();
    }

    @Override
    public synchronized void register(GameEventListener listener) {
        listeners.add(listener);
    }

    @Override
    public synchronized void unregister(GameEventListener listener) {
        listeners.remove(listener);
    }

    @Override
    public boolean visitInRangeListeners(Holder<GameEvent> event, Vec3 sourcePosition, GameEvent.Context context, ListenerVisitor action) {
        List<GameEventListener> snapshot;
        synchronized (this) {
            snapshot = List.copyOf(listeners);
        }
        boolean visited = false;
        for (GameEventListener listener : snapshot) {
            Optional<Vec3> at = listener.getListenerSource().getPosition(level);
            if (at.isPresent() && at.get().distanceToSqr(sourcePosition) <= (double) listener.getListenerRadius() * listener.getListenerRadius()) {
                action.visit(listener, at.get());
                visited = true;
            }
        }
        return visited;
    }
}
