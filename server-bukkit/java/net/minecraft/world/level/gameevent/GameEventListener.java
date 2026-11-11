// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.gameevent;
import net.minecraft.core.Holder;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.world.phys.Vec3;
public interface GameEventListener {
    PositionSource getListenerSource();
    int getListenerRadius();
    boolean handleGameEvent(ServerLevel level, Holder<GameEvent> event, GameEvent.Context context, Vec3 sourcePosition);
    default DeliveryMode getDeliveryMode() {
        return DeliveryMode.UNSPECIFIED;
    }
    public static enum DeliveryMode {
        UNSPECIFIED, BY_DISTANCE;
    }
    public interface Provider<T extends GameEventListener> {
        T getListener();
    }
}
