// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.ai.behavior;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.world.entity.LivingEntity;
import net.minecraft.world.entity.ai.behavior.declarative.Trigger;
public abstract class OneShot<E extends LivingEntity> implements BehaviorControl<E>, Trigger<E> {
    private Behavior.Status status = Behavior.Status.STOPPED;
    public OneShot() {
    }
    public final Behavior.Status getStatus() {
        return status;
    }
    public final boolean tryStart(ServerLevel level, E body, long timestamp) {
        if (trigger(level, body, timestamp)) {
            status = Behavior.Status.RUNNING;
            return true;
        }
        return false;
    }
    public final void tickOrStop(ServerLevel level, E body, long timestamp) {
        doStop(level, body, timestamp);
    }
    public final void doStop(ServerLevel level, E body, long timestamp) {
        status = Behavior.Status.STOPPED;
    }
    public String debugString() {
        return getClass().getSimpleName();
    }
}
