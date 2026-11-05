// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.gameevent;

import net.minecraft.core.Holder;
import net.minecraft.world.phys.Vec3;
public interface GameEventListenerRegistry {
    GameEventListenerRegistry NOOP = new GameEventListenerRegistry() {
        @Override
        public boolean isEmpty() {
            return true;
        }

        @Override
        public void register(GameEventListener listener) {
        }

        @Override
        public void unregister(GameEventListener listener) {
        }

        @Override
        public boolean visitInRangeListeners(Holder<GameEvent> event, Vec3 sourcePosition, GameEvent.Context context, ListenerVisitor action) {
            return false;
        }
    };
    default boolean isEmpty() {
        return true;
    }
    default void register(GameEventListener listener) {
        return;
    }
    default void unregister(GameEventListener listener) {
        return;
    }
    default boolean visitInRangeListeners(Holder<GameEvent> event, Vec3 sourcePosition, GameEvent.Context context, ListenerVisitor action) {
        return false;
    }
    public interface ListenerVisitor {
        void visit(GameEventListener listener, Vec3 position);
    }
}
