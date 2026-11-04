// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.gameevent;

import net.minecraft.core.Holder;
import net.minecraft.world.phys.Vec3;
public interface GameEventListenerRegistry {
    GameEventListenerRegistry NOOP = null;
    default boolean isEmpty() {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.gameevent.GameEventListenerRegistry#isEmpty");
    }
    default void register(GameEventListener listener) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.gameevent.GameEventListenerRegistry#register");
    }
    default void unregister(GameEventListener listener) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.gameevent.GameEventListenerRegistry#unregister");
    }
    default boolean visitInRangeListeners(Holder<GameEvent> event, Vec3 sourcePosition, GameEvent.Context context, ListenerVisitor action) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.gameevent.GameEventListenerRegistry#visitInRangeListeners");
    }
    public interface ListenerVisitor {
        void visit(GameEventListener listener, Vec3 position);
    }
}
