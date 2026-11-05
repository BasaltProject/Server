package org.basaltmc.bukkit;

import java.util.HashMap;
import java.util.IdentityHashMap;
import java.util.Map;
import net.minecraft.world.entity.LivingEntity;
import net.minecraft.world.entity.ai.behavior.BehaviorControl;

/** Answers the native brain's behavior calls with the Java behavior objects a plugin registered on a brain. */
public final class NativeBehaviors {
    public static final NativeBehaviors INSTANCE = new NativeBehaviors();

    private static final int kPruneAbove = 256;

    private record Bound(LivingEntity body, BehaviorControl<LivingEntity> behavior) {}

    private final Map<String, Bound> bound = new HashMap<>();
    private final Map<BehaviorControl<?>, String> keys = new IdentityHashMap<>();
    private boolean listening;
    private long serial;

    private NativeBehaviors() {}

    private static String holder(LivingEntity body) {
        return body.level().getWorld().getName() + "/" + body.getId();
    }

    /** The key the native row carries: unique per behavior object, so two instances of one class never share a run. */
    @SuppressWarnings("unchecked")
    public synchronized String bind(LivingEntity body, BehaviorControl<?> behavior) {
        String known = keys.get(behavior);
        if (known != null) {
            return known;
        }
        String key = behavior.getClass().getSimpleName() + "#" + (++serial);
        keys.put(behavior, key);
        bound.put(holder(body) + "|" + key, new Bound(body, (BehaviorControl<LivingEntity>) behavior));
        if (!listening) {
            listening = Natives.subscribe("entity.behavior", org.bukkit.event.EventPriority.MONITOR.ordinal());
        }
        prune();
        return key;
    }

    public synchronized void unbind(LivingEntity body, BehaviorControl<?> behavior) {
        String key = keys.remove(behavior);
        if (key != null) {
            bound.remove(holder(body) + "|" + key);
        }
    }

    private void prune() {
        if (bound.size() <= kPruneAbove) {
            return;
        }
        bound.entrySet().removeIf(entry -> {
            LivingEntity body = entry.getValue().body();
            boolean gone = body.isRemoved();
            if (gone) {
                keys.remove(entry.getValue().behavior());
            }
            return gone;
        });
    }

    /** Runs one phase of a behavior the native brain asked about; the answer is false once the behavior is gone. */
    synchronized void answer(EventBridge.Fields f) {
        Bound found = bound.get(f.text("world") + "/" + f.whole("entity_id") + "|" + f.text("key"));
        if (found == null) {
            f.put("result", false);
            return;
        }
        LivingEntity body = found.body();
        BehaviorControl<LivingEntity> behavior = found.behavior();
        long time = body.level().getGameTime();
        try {
            switch (f.text("phase")) {
                case "can_use" -> f.put("result", behavior.tryStart(body.level(), body, time));
                case "can_continue" -> f.put("result", behavior.getStatus() == net.minecraft.world.entity.ai.behavior.Behavior.Status.RUNNING);
                case "tick" -> behavior.tickOrStop(body.level(), body, time);
                case "stop" -> {
                    if (behavior.getStatus() == net.minecraft.world.entity.ai.behavior.Behavior.Status.RUNNING) {
                        behavior.doStop(body.level(), body, time);
                    }
                }
                default -> { }
            }
        } catch (Throwable failure) {
            BasaltServer.instance().getLogger().severe("Brain behavior " + f.text("key") + " failed: " + failure);
            f.put("result", false);
        }
    }
}
