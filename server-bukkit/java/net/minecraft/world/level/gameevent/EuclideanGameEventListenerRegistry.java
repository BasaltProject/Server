// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.gameevent;
import java.util.List;
import java.util.Set;
import java.util.Optional;
import java.util.ArrayList;
import java.util.Iterator;
import net.minecraft.core.BlockPos;
import java.util.HashSet;
import net.minecraft.core.Holder;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.world.phys.Vec3;
public class EuclideanGameEventListenerRegistry implements GameEventListenerRegistry {
    private Optional<Vec3> reachablePosition(Vec3 sourcePosition, GameEventListener listener) {
        Optional<Vec3> position = listener.getListenerSource().getPosition(level);
        if (position.isEmpty()) {
            return position;
        }
        int radius = listener.getListenerRadius();
        double distance = BlockPos.containing(position.get()).distSqr(BlockPos.containing(sourcePosition));
        return distance > (double) radius * radius ? Optional.empty() : position;
    }
    private final List<GameEventListener> listeners = new ArrayList<>();
    private final Set<GameEventListener> pendingRemoval = new HashSet<>();
    private final List<GameEventListener> pendingAdd = new ArrayList<>();
    private boolean visiting;
    private final ServerLevel level;
    private final int sectionY;
    private final OnEmptyAction onEmptyAction;
    public EuclideanGameEventListenerRegistry(ServerLevel level, int sectionY, OnEmptyAction onEmptyAction) {
        this.level = level;
        this.sectionY = sectionY;
        this.onEmptyAction = onEmptyAction;
    }
    public boolean isEmpty() {
        return listeners.isEmpty();
    }
    public void register(GameEventListener listener) {
        (visiting ? pendingAdd : listeners).add(listener);
    }
    public void unregister(GameEventListener listener) {
        if (visiting) {
            pendingRemoval.add(listener);
        } else {
            listeners.remove(listener);
        }
        if (listeners.isEmpty()) {
            onEmptyAction.apply(sectionY);
        }
    }
    public boolean visitInRangeListeners(Holder<GameEvent> event, Vec3 sourcePosition, GameEvent.Context context, GameEventListenerRegistry.ListenerVisitor action) {
        visiting = true;
        boolean reached = false;
        try {
            for (Iterator<GameEventListener> each = listeners.iterator(); each.hasNext(); ) {
                GameEventListener listener = each.next();
                if (pendingRemoval.remove(listener)) {
                    each.remove();
                    continue;
                }
                Optional<Vec3> where = reachablePosition(sourcePosition, listener);
                if (where.isPresent()) {
                    action.visit(listener, where.get());
                    reached = true;
                }
            }
        } finally {
            visiting = false;
        }
        listeners.addAll(pendingAdd);
        pendingAdd.clear();
        listeners.removeAll(pendingRemoval);
        pendingRemoval.clear();
        return reached;
    }
    public interface OnEmptyAction {
        void apply(int sectionY);
    }
}
