package org.basaltmc.bukkit;

import com.destroystokyo.paper.entity.ai.Goal;
import com.destroystokyo.paper.entity.ai.GoalKey;
import com.destroystokyo.paper.entity.ai.GoalType;
import com.destroystokyo.paper.entity.ai.MobGoals;
import java.util.ArrayList;
import java.util.Collection;
import java.util.EnumSet;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.logging.Level;
import org.basaltmc.bukkit.entity.Cored;
import org.basaltmc.bukkit.entity.EntityCore;
import org.bukkit.NamespacedKey;
import org.bukkit.entity.Mob;

/** Mob goals over the plugin goal hook of the mob AI: one goal per key and mob, vanilla ones seen by name. */
public final class BasaltMobGoals implements MobGoals {
    public static final BasaltMobGoals INSTANCE = new BasaltMobGoals();

    private static final int kPruneAbove = 256;

    private final Map<String, Map<String, Goal<?>>> owned = new HashMap<>();
    private final Map<String, Goal<?>> retiring = new HashMap<>();
    private boolean listening;

    private BasaltMobGoals() {}

    private static EntityCore coreOf(Mob mob) {
        return ((Cored) mob).core();
    }

    private static String holder(EntityCore core) {
        return core.world.getName() + "/" + core.id;
    }

    private static String keyOf(GoalKey<?> key) {
        return key.getNamespacedKey().toString();
    }

    private static String flagsOf(EnumSet<GoalType> types) {
        StringBuilder flags = new StringBuilder();
        for (GoalType type : types) {
            if (type != GoalType.UNKNOWN_BEHAVIOR) {
                flags.append(flags.length() == 0 ? "" : ",").append(type.name().toLowerCase(java.util.Locale.ROOT));
            }
        }
        return flags.toString();
    }

    private static EnumSet<GoalType> typesOf(String flags) {
        EnumSet<GoalType> types = EnumSet.noneOf(GoalType.class);
        for (String flag : flags.split(",")) {
            if (!flag.isEmpty()) {
                types.add(GoalType.valueOf(flag.toUpperCase(java.util.Locale.ROOT)));
            }
        }
        if (types.isEmpty()) {
            types.add(GoalType.UNKNOWN_BEHAVIOR);
        }
        return types;
    }

    private record Row(String key, int priority, boolean running, boolean plugin, boolean usable,
            EnumSet<GoalType> types) {}

    private static List<Row> rows(EntityCore core) {
        List<Row> out = new ArrayList<>();
        if (core.call("goals") instanceof String[] flat) {
            for (int at = 0; at + 6 <= flat.length; at += 6) {
                out.add(new Row(flat[at], Integer.parseInt(flat[at + 1]), flat[at + 2].equals("1"),
                    flat[at + 3].equals("1"), flat[at + 4].equals("1"), typesOf(flat[at + 5])));
            }
        }
        return out;
    }

    private static NamespacedKey namespaced(String key) {
        NamespacedKey parsed = NamespacedKey.fromString(key);
        return parsed != null ? parsed : NamespacedKey.minecraft(key);
    }

    @SuppressWarnings({"unchecked", "rawtypes"})
    private Goal<?> view(Mob mob, Row row) {
        Map<String, Goal<?>> mine = owned.get(holder(coreOf(mob)));
        Goal<?> custom = mine == null ? null : mine.get(namespaced(row.key).toString());
        if (row.plugin && custom != null) {
            return custom;
        }
        return new VanillaGoalView(mob, row);
    }

    private static final class VanillaGoalView implements Goal<Mob> {
        private final GoalKey<Mob> key;
        private final EnumSet<GoalType> types;
        private final boolean usable;

        @SuppressWarnings("unchecked")
        VanillaGoalView(Mob mob, Row row) {
            this.key = GoalKey.of((Class<Mob>) (Class<?>) Mob.class, namespaced(row.key));
            this.types = row.types;
            this.usable = row.usable;
        }

        @Override
        public boolean shouldActivate() {
            return usable;
        }

        @Override
        public GoalKey<Mob> getKey() {
            return key;
        }

        @Override
        public EnumSet<GoalType> getTypes() {
            return types;
        }
    }

    private void retire(EntityCore core, String key, Goal<?> goal) {
        if (goal == null) {
            return;
        }
        for (Row row : rows(core)) {
            if (row.plugin && row.running && namespaced(row.key).toString().equals(namespaced(key).toString())) {
                retiring.put(holder(core) + "|" + namespaced(key), goal);
                return;
            }
        }
    }

    private void prune() {
        if (owned.size() <= kPruneAbove) {
            return;
        }
        owned.keySet().removeIf(id -> {
            int slash = id.lastIndexOf('/');
            BasaltWorld world = BasaltServer.instance().world(id.substring(0, slash));
            return world == null || BasaltServer.instance().entityById(world, Integer.parseInt(id.substring(slash + 1)))
                == null;
        });
    }

    @Override
    @SuppressWarnings("unchecked")
    public <T extends Mob> void addGoal(T mob, int priority, Goal<T> goal) {
        EntityCore core = coreOf(mob);
        String key = keyOf(goal.getKey());
        Map<String, Goal<?>> mine = owned.computeIfAbsent(holder(core), id -> new HashMap<>());
        Goal<?> replaced = mine.put(key, goal);
        if (replaced != null) {
            retire(core, key, replaced);
            core.call("remove_goal", key);
        }
        if (!listening) {
            listening = Natives.subscribe("entity.goal", org.bukkit.event.EventPriority.MONITOR.ordinal());
        }
        core.call("add_goal", key, (long) priority, flagsOf(goal.getTypes()));
        prune();
    }

    @Override
    public <T extends Mob> void removeGoal(T mob, Goal<T> goal) {
        EntityCore core = coreOf(mob);
        Map<String, Goal<?>> mine = owned.get(holder(core));
        String key = keyOf(goal.getKey());
        if (mine != null && mine.get(key) == goal) {
            retire(core, key, mine.remove(key));
            core.call("remove_goal", key);
        }
    }

    @Override
    public <T extends Mob> void removeAllGoals(T mob) {
        EntityCore core = coreOf(mob);
        Map<String, Goal<?>> gone = owned.remove(holder(core));
        if (gone != null) {
            gone.forEach((key, goal) -> retire(core, key, goal));
        }
        core.call("remove_goals", "");
    }

    @Override
    public <T extends Mob> void removeAllGoals(T mob, GoalType type) {
        EntityCore core = coreOf(mob);
        String flag = flagsOf(EnumSet.of(type));
        Map<String, Goal<?>> mine = owned.get(holder(core));
        if (mine != null) {
            mine.entrySet().removeIf(entry -> {
                boolean match = entry.getValue().getTypes().contains(type);
                if (match) {
                    retire(core, entry.getKey(), entry.getValue());
                }
                return match;
            });
        }
        if (!flag.isEmpty()) {
            core.call("remove_goals", flag);
        }
    }

    @Override
    public <T extends Mob> void removeGoal(T mob, GoalKey<T> key) {
        EntityCore core = coreOf(mob);
        Map<String, Goal<?>> mine = owned.get(holder(core));
        if (mine != null) {
            retire(core, keyOf(key), mine.remove(keyOf(key)));
        }
        core.call("remove_goal", keyOf(key));
    }

    @Override
    public <T extends Mob> boolean hasGoal(T mob, GoalKey<T> key) {
        return coreOf(mob).test("has_goal", keyOf(key));
    }

    @Override
    @SuppressWarnings("unchecked")
    public <T extends Mob> Goal<T> getGoal(T mob, GoalKey<T> key) {
        String wanted = keyOf(key);
        for (Row row : rows(coreOf(mob))) {
            if (namespaced(row.key).toString().equals(wanted)) {
                return (Goal<T>) view(mob, row);
            }
        }
        return null;
    }

    @Override
    @SuppressWarnings("unchecked")
    public <T extends Mob> Collection<Goal<T>> getGoals(T mob, GoalKey<T> key) {
        Goal<T> found = getGoal(mob, key);
        return found == null ? List.of() : List.of(found);
    }

    @SuppressWarnings("unchecked")
    private <T extends Mob> Collection<Goal<T>> select(T mob, boolean runningOnly, GoalType type, boolean without) {
        List<Goal<T>> out = new ArrayList<>();
        for (Row row : rows(coreOf(mob))) {
            if (runningOnly && !row.running) {
                continue;
            }
            if (type != null && row.types.contains(type) == without) {
                continue;
            }
            out.add((Goal<T>) view(mob, row));
        }
        return out;
    }

    @Override
    public <T extends Mob> Collection<Goal<T>> getAllGoals(T mob) {
        return select(mob, false, null, false);
    }

    @Override
    public <T extends Mob> Collection<Goal<T>> getAllGoals(T mob, GoalType type) {
        return select(mob, false, type, false);
    }

    @Override
    public <T extends Mob> Collection<Goal<T>> getAllGoalsWithout(T mob, GoalType type) {
        return select(mob, false, type, true);
    }

    @Override
    public <T extends Mob> Collection<Goal<T>> getRunningGoals(T mob) {
        return select(mob, true, null, false);
    }

    @Override
    public <T extends Mob> Collection<Goal<T>> getRunningGoals(T mob, GoalType type) {
        return select(mob, true, type, false);
    }

    @Override
    public <T extends Mob> Collection<Goal<T>> getRunningGoalsWithout(T mob, GoalType type) {
        return select(mob, true, type, true);
    }

    /** Runs the decision of a plugin goal the mob AI asked about; false when the goal is gone. */
    @SuppressWarnings({"unchecked", "rawtypes"})
    void answer(EventBridge.Fields f) {
        Map<String, Goal<?>> mine = owned.get(f.text("world") + "/" + f.whole("entity_id"));
        String key = namespaced(f.text("key")).toString();
        Goal goal = mine == null ? null : mine.get(key);
        if (goal == null) {
            String stored = f.text("world") + "/" + f.whole("entity_id") + "|" + key;
            Goal gone = f.text("phase").equals("stop") ? retiring.remove(stored) : null;
            if (gone != null) {
                try {
                    gone.stop();
                } catch (Throwable failure) {
                    BasaltServer.instance().getLogger().severe("Mob goal " + key + " failed: " + failure);
                }
            }
            return;
        }
        try {
            switch (f.text("phase")) {
                case "can_use" -> f.put("result", goal.shouldActivate());
                case "can_continue" -> f.put("result", goal.shouldStayActive());
                case "start" -> goal.start();
                case "stop" -> goal.stop();
                default -> goal.tick();
            }
        } catch (Throwable failure) {
            BasaltServer.instance().getLogger().severe("Mob goal " + f.text("key") + " failed: " + failure);
            f.put("result", false);
        }
    }
}
