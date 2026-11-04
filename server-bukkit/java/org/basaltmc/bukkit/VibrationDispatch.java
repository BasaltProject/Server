package org.basaltmc.bukkit;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.UUID;
import net.minecraft.core.Holder;
import net.minecraft.core.registries.BuiltInRegistries;
import net.minecraft.resources.Identifier;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.world.entity.Entity;
import net.minecraft.world.level.gameevent.GameEvent;
import net.minecraft.world.level.gameevent.vibrations.VibrationSystem;
import net.minecraft.world.phys.Vec3;

/** Feeds the native game event bus to the vibration listeners of the Warden and Allay mirrors. */
final class VibrationDispatch {
    private static final class Tracked {
        final VibrationSystem system;
        final ServerLevel level;
        int ticks;
        boolean seen;

        Tracked(VibrationSystem system, ServerLevel level) {
            this.system = system;
            this.level = level;
        }
    }

    private static final Map<UUID, VibrationSystem.Listener> kListeners = new HashMap<>();
    private static final Map<UUID, Tracked> kTracked = new HashMap<>();

    private VibrationDispatch() {}

    private static String kLastSignature = "";
    private static final long kRescanTicks = 20;
    private static final Map<String, Long> kScanned = new HashMap<>();
    private static final Map<String, List<org.bukkit.entity.Entity>> kFound = new HashMap<>();

    private static List<org.bukkit.entity.Entity> listeners(BasaltWorld world) {
        long now = Natives.currentTick();
        Long last = kScanned.get(world.getName());
        if (last == null || now - last >= kRescanTicks) {
            List<org.bukkit.entity.Entity> found = new ArrayList<>();
            for (org.bukkit.entity.Entity candidate : world.getEntities()) {
                if (candidate instanceof org.bukkit.entity.Warden || candidate instanceof org.bukkit.entity.Allay) {
                    found.add(candidate);
                }
            }
            kFound.put(world.getName(), found);
            kScanned.put(world.getName(), now);
        }
        return kFound.get(world.getName());
    }

    static void install() {
        EventBridge.mapAlways("world.game_event", VibrationDispatch::deliver);
    }

    private static synchronized void deliver(EventBridge.Fields f) {
        BasaltWorld world = f.world();
        if (world == null) {
            return;
        }
        String signature = Natives.currentTick() + "|" + world.getName() + "|" + f.text("event") + "|" + f.real("x") + "|"
            + f.real("y") + "|" + f.real("z") + "|" + f.text("entity_uuid");
        if (signature.equals(kLastSignature)) {
            return;
        }
        kLastSignature = signature;
        Holder<GameEvent> event = BuiltInRegistries.GAME_EVENT.get(Identifier.withDefaultNamespace(f.text("event")))
            .map(found -> (Holder<GameEvent>) found).orElse(null);
        if (event == null) {
            return;
        }
        Vec3 at = new Vec3(f.real("x"), f.real("y"), f.real("z"));
        String uuid = f.text("entity_uuid");
        org.bukkit.entity.Entity sourceLive = uuid.isEmpty() ? null : world.getEntity(UUID.fromString(uuid));
        GameEvent.Context context = GameEvent.Context.of(sourceLive == null ? null : Entity.fromBukkit(sourceLive));
        for (org.bukkit.entity.Entity candidate : listeners(world)) {
            if (!(Entity.fromBukkit(candidate) instanceof VibrationSystem system)
                || !(Entity.fromBukkit(candidate).level() instanceof ServerLevel level)) {
                continue;
            }
            Vec3 target = system.getVibrationUser().getPositionSource().getPosition(level).orElse(null);
            int radius = system.getVibrationUser().getListenerRadius();
            if (target == null || target.distanceToSqr(at) > (double) radius * radius) {
                continue;
            }
            UUID id = candidate.getUniqueId();
            VibrationSystem.Listener listener = kListeners.computeIfAbsent(id, key -> new VibrationSystem.Listener(system));
            if (listener.handleGameEvent(level, event, context, at)) {
                kTracked.put(id, new Tracked(system, level));
            }
        }
    }

    static synchronized void tick() {
        List<UUID> done = new ArrayList<>();
        kTracked.forEach((id, tracked) -> {
            VibrationSystem.Data data = tracked.system.getVibrationData();
            VibrationSystem.Ticker.tick(tracked.level, data, tracked.system.getVibrationUser());
            tracked.seen |= data.getCurrentVibration() != null;
            if (data.getCurrentVibration() == null && (tracked.seen || ++tracked.ticks > 5)) {
                done.add(id);
            }
        });
        done.forEach(kTracked::remove);
    }
}
