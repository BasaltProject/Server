package org.basaltmc.bukkit;

import java.util.Collections;
import java.util.LinkedHashMap;
import java.util.Map;
import java.util.UUID;
import java.util.concurrent.ConcurrentHashMap;
import java.util.function.Predicate;
import net.kyori.adventure.key.Key;
import org.bukkit.entity.Entity;
import org.bukkit.event.Event;
import org.bukkit.event.EventPriority;
import org.bukkit.event.HandlerList;

public final class AlliedRules {
    private static final String kEvent = "entity.allied_check";
    private static final Map<UUID, Map<Key, Predicate<Entity>>> rules = new ConcurrentHashMap<>();
    private static volatile boolean subscribed;

    private AlliedRules() {}

    /** The event the bridge builds so the answer can be written back; no plugin ever hears it. */
    private static final class Check extends Event {
        private static final HandlerList handlers = new HandlerList();

        @Override
        public HandlerList getHandlers() {
            return handlers;
        }
    }

    static void install() {
        EventBridge.map(kEvent, Check.class, f -> new Check(), (event, f) -> {
            Entity attacker = f.entity();
            BasaltWorld world = f.world();
            Entity other = world == null ? null : world.entity((int) f.whole("other_id"));
            f.put("allied", attacker != null && other != null && matches(attacker, other));
        });
    }

    public static Map<Key, Predicate<Entity>> of(Entity entity) {
        Map<Key, Predicate<Entity>> held = rules.get(entity.getUniqueId());
        return held == null ? Map.of() : Collections.unmodifiableMap(held);
    }

    public static void add(Entity entity, Key key, Predicate<Entity> rule) {
        if (key == null || rule == null) {
            throw new NullPointerException("key and rule cannot be null");
        }
        synchronized (rules) {
            rules.computeIfAbsent(entity.getUniqueId(), id -> new LinkedHashMap<>()).put(key, rule);
        }
        if (!subscribed) {
            subscribed = true;
            Natives.subscribe(kEvent, EventPriority.NORMAL.ordinal());
        }
    }

    public static void remove(Entity entity, Key key) {
        synchronized (rules) {
            Map<Key, Predicate<Entity>> held = rules.get(entity.getUniqueId());
            if (held != null) {
                held.remove(key);
                if (held.isEmpty()) {
                    rules.remove(entity.getUniqueId());
                }
            }
        }
    }

    public static void removeNamespace(Entity entity, String namespace) {
        synchronized (rules) {
            Map<Key, Predicate<Entity>> held = rules.get(entity.getUniqueId());
            if (held != null) {
                held.keySet().removeIf(key -> key.namespace().equals(namespace));
                if (held.isEmpty()) {
                    rules.remove(entity.getUniqueId());
                }
            }
        }
    }

    public static boolean matches(Entity entity, Entity other) {
        Map<Key, Predicate<Entity>> held = rules.get(entity.getUniqueId());
        if (held == null) {
            return false;
        }
        for (Predicate<Entity> rule : java.util.List.copyOf(held.values())) {
            if (rule.test(other)) {
                return true;
            }
        }
        return false;
    }
}
