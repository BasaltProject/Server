// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.ai;

import com.google.common.collect.ImmutableList;
import com.google.common.collect.ImmutableSet;
import com.mojang.datafixers.util.Pair;
import com.mojang.serialization.Codec;
import com.mojang.serialization.codecs.RecordCodecBuilder;
import java.util.ArrayList;
import java.util.Collection;
import java.util.HashMap;
import java.util.HashSet;
import java.util.LinkedHashMap;
import java.util.LinkedHashSet;
import java.util.List;
import java.util.Map;
import java.util.Objects;
import java.util.Optional;
import java.util.Set;
import java.util.TreeMap;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.util.RandomSource;
import net.minecraft.world.attribute.EnvironmentAttribute;
import net.minecraft.world.attribute.EnvironmentAttributeSystem;
import net.minecraft.world.entity.LivingEntity;
import net.minecraft.world.entity.ai.behavior.Behavior;
import net.minecraft.world.entity.ai.behavior.BehaviorControl;
import net.minecraft.world.entity.ai.memory.ExpirableValue;
import net.minecraft.world.entity.ai.memory.MemoryMap;
import net.minecraft.world.entity.ai.memory.MemoryModuleType;
import net.minecraft.world.entity.ai.memory.MemoryStatus;
import net.minecraft.world.entity.ai.sensing.Sensor;
import net.minecraft.world.entity.ai.sensing.SensorType;
import net.minecraft.world.entity.schedule.Activity;
import net.minecraft.world.phys.Vec3;
import org.basaltmc.bukkit.NativeBehaviors;
import org.basaltmc.bukkit.NativeBrain;

public class Brain<E extends LivingEntity> {
    private final NativeBrain nativeBrain;
    private final LivingEntity body;
    private final Map<MemoryModuleType<?>, Object[]> payloads = new HashMap<>();
    private final Set<MemoryModuleType<?>> registered = new LinkedHashSet<>();
    private final Map<SensorType<? extends Sensor<? super E>>, Sensor<? super E>> sensors = new LinkedHashMap<>();
    private final Map<Integer, Map<Activity, Set<BehaviorControl<? super E>>>> availableBehaviorsByPriority = new TreeMap<>();
    private final Map<String, Activity> activitiesByName = new HashMap<>();
    private EnvironmentAttribute<Activity> schedule;
    private Activity defaultActivity = Activity.IDLE;
    private long lastScheduleUpdate = -9999L;

    public static <E extends LivingEntity> Provider<E> provider(Collection<? extends SensorType<? extends Sensor<? super E>>> sensorTypes) {
        return new Provider<>(ImmutableList.of(), sensorTypes, body -> List.of());
    }

    public static <E extends LivingEntity> Provider<E> provider(Collection<? extends SensorType<? extends Sensor<? super E>>> sensorTypes,
        ActivitySupplier<E> activities) {
        return new Provider<>(ImmutableList.of(), sensorTypes, activities);
    }

    @Deprecated
    public static <E extends LivingEntity> Provider<E> provider(Collection<? extends MemoryModuleType<?>> memoryTypes,
        Collection<? extends SensorType<? extends Sensor<? super E>>> sensorTypes, ActivitySupplier<E> activities) {
        return new Provider<>(memoryTypes, sensorTypes, activities);
    }

    protected Brain(E body, Collection<? extends MemoryModuleType<?>> memoryTypes,
        Collection<? extends SensorType<? extends Sensor<? super E>>> sensorTypes, List<ActivityData<E>> activities, MemoryMap memories,
        RandomSource randomSource) {
        this.body = body;
        nativeBrain = new NativeBrain(body);
        for (MemoryModuleType<?> memoryType : memoryTypes) {
            registerMemory(memoryType);
        }
        for (SensorType<? extends Sensor<? super E>> sensorType : sensorTypes) {
            Sensor<? super E> sensor = sensorType.create();
            sensor.randomlyDelayStart(randomSource);
            this.sensors.put(sensorType, sensor);
            for (MemoryModuleType<?> type : sensor.requires()) {
                registerMemory(type);
            }
        }
        for (ActivityData<E> activity : activities) {
            addActivity(activity.activityType(), activity.behaviorPriorityPairs(), activity.conditions(), activity.memoriesToEraseWhenStopped());
        }
        for (MemoryMap.Value<?> memory : memories) {
            setMemoryInternal(memory);
        }
        setCoreActivities(ImmutableSet.of(Activity.CORE));
        useDefaultActivity();
    }

    public Brain(E body) {
        this.body = body;
        nativeBrain = new NativeBrain(body);
        setCoreActivities(ImmutableSet.of(Activity.CORE));
        useDefaultActivity();
    }

    private static String keyOf(MemoryModuleType<?> type) {
        String name = type.toString();
        return name.startsWith("minecraft:") ? name.substring("minecraft:".length()) : name;
    }

    private static String nameOf(Activity activity) {
        return activity.getName();
    }

    private Activity activityOf(String name) {
        Activity found = activitiesByName.get(name);
        return found != null ? found : Activity.registry().getValue(net.minecraft.resources.Identifier.withDefaultNamespace(name));
    }

    private void registerMemory(MemoryModuleType<?> memoryType) {
        if (registered.add(memoryType)) {
            nativeBrain.call("brain_register", keyOf(memoryType));
        }
    }

    public Packed pack() {
        MemoryMap.Builder builder = new MemoryMap.Builder();
        forEach(new Visitor() {
            @Override
            public <U> void acceptEmpty(MemoryModuleType<U> type) {
            }

            @Override
            public <U> void accept(MemoryModuleType<U> type, U value, long timeToLive) {
                if (type.canSerialize()) {
                    builder.add(type, ExpirableValue.of(value, timeToLive));
                }
            }

            @Override
            public <U> void accept(MemoryModuleType<U> type, U value) {
                if (type.canSerialize()) {
                    builder.add(type, ExpirableValue.of(value));
                }
            }
        });
        return new Packed(builder.build());
    }

    private void requireRegistered(MemoryModuleType<?> memoryType) {
        if (!registered.contains(memoryType)) {
            throw new IllegalStateException("Unregistered memory fetched: " + memoryType);
        }
    }

    public boolean hasMemoryValue(MemoryModuleType<?> type) {
        return checkMemory(type, MemoryStatus.VALUE_PRESENT);
    }

    public void clearMemories() {
        payloads.clear();
        nativeBrain.call("brain_clear_memories");
    }

    public <U> void eraseMemory(MemoryModuleType<U> type) {
        if (registered.contains(type)) {
            payloads.remove(type);
            nativeBrain.call("brain_erase_memory", keyOf(type));
        }
    }

    public <U> void setMemory(MemoryModuleType<U> type, U value) {
        setMemoryInternal(type, value, -1L);
    }

    public <U> void setMemoryWithExpiry(MemoryModuleType<U> type, U value, long timeToLive) {
        setMemoryInternal(type, value, timeToLive);
    }

    public <U> void setMemory(MemoryModuleType<U> type, Optional<? extends U> optionalValue) {
        setMemoryInternal(type, optionalValue.orElse(null), -1L);
    }

    private <U> void setMemoryInternal(MemoryMap.Value<U> value) {
        ExpirableValue<U> expirable = value.value();
        setMemoryInternal(value.type(), expirable.value(), expirable.timeToLive().isPresent() ? expirable.timeToLive().get() : -1L);
    }

    private static boolean isEmptyCollection(Object value) {
        return value instanceof Collection<?> collection && collection.isEmpty();
    }

    private <U> void setMemoryInternal(MemoryModuleType<U> type, U value, long timeToLive) {
        if (!registered.contains(type)) {
            return;
        }
        if (value == null || isEmptyCollection(value)) {
            eraseMemory(type);
            return;
        }
        Object encoded = nativeBrain.encode(value);
        payloads.put(type, new Object[] {value, NativeBrain.fingerprint(encoded)});
        nativeBrain.call("brain_set_memory", keyOf(type), encoded, timeToLive);
    }

    @SuppressWarnings("unchecked")
    private <U> U read(MemoryModuleType<U> type) {
        Object wire = nativeBrain.call("brain_memory", keyOf(type));
        if (wire == null) {
            payloads.remove(type);
            return null;
        }
        Object[] held = payloads.get(type);
        if (held != null && held[1].equals(NativeBrain.fingerprint(wire))) {
            return (U) held[0];
        }
        payloads.remove(type);
        return (U) nativeBrain.decode(wire);
    }

    public <U> Optional<U> getMemory(MemoryModuleType<U> type) {
        requireRegistered(type);
        return Optional.ofNullable(read(type));
    }

    public <U> Optional<U> getMemoryInternal(MemoryModuleType<U> type) {
        return registered.contains(type) ? Optional.ofNullable(read(type)) : null;
    }

    public <U> long getTimeUntilExpiry(MemoryModuleType<U> type) {
        requireRegistered(type);
        return nativeBrain.call("brain_memory_ttl", keyOf(type)) instanceof Long left && left >= 0 ? left : Long.MAX_VALUE;
    }

    public void forEach(Visitor visitor) {
        for (MemoryModuleType<?> type : registered) {
            visit(visitor, type);
        }
    }

    @SuppressWarnings("unchecked")
    private <U> void visit(Visitor visitor, MemoryModuleType<U> type) {
        U value = read(type);
        if (value == null) {
            visitor.acceptEmpty(type);
            return;
        }
        long left = getTimeUntilExpiry(type);
        if (left == Long.MAX_VALUE) {
            visitor.accept(type, value);
        } else {
            visitor.accept(type, value, left);
        }
    }

    public <U> boolean isMemoryValue(MemoryModuleType<U> memoryType, U value) {
        return registered.contains(memoryType) && Objects.equals(value, read(memoryType));
    }

    public boolean checkMemory(MemoryModuleType<?> type, MemoryStatus status) {
        if (!registered.contains(type)) {
            return false;
        }
        return status == MemoryStatus.REGISTERED || (status == MemoryStatus.VALUE_PRESENT && nativeBrain.test("brain_has_memory", keyOf(type)))
            || (status == MemoryStatus.VALUE_ABSENT && !nativeBrain.test("brain_has_memory", keyOf(type)));
    }

    public void setSchedule(EnvironmentAttribute<Activity> schedule) {
        this.schedule = schedule;
        String native_ = schedule == net.minecraft.world.attribute.EnvironmentAttributes.VILLAGER_ACTIVITY ? "villager" : "";
        nativeBrain.call("brain_schedule", native_);
    }

    public void setCoreActivities(Set<Activity> activities) {
        String[] names = activities.stream().map(Brain::nameOf).toArray(String[]::new);
        nativeBrain.call("brain_set_core", (Object) names);
    }

    @Deprecated
    public Set<Activity> getActiveActivities() {
        Set<Activity> out = new HashSet<>();
        for (String name : nativeBrain.strings("brain_active")) {
            Activity activity = activityOf(name);
            if (activity != null) {
                out.add(activity);
            }
        }
        return out;
    }

    @Deprecated
    public List<BehaviorControl<? super E>> getRunningBehaviors() {
        List<BehaviorControl<? super E>> running = new ArrayList<>();
        for (Map<Activity, Set<BehaviorControl<? super E>>> byActivity : availableBehaviorsByPriority.values()) {
            for (Set<BehaviorControl<? super E>> behaviors : byActivity.values()) {
                for (BehaviorControl<? super E> behavior : behaviors) {
                    if (behavior.getStatus() == Behavior.Status.RUNNING) {
                        running.add(behavior);
                    }
                }
            }
        }
        return running;
    }

    public void useDefaultActivity() {
        nativeBrain.call("brain_set_activity", nameOf(defaultActivity), "default");
    }

    public Optional<Activity> getActiveNonCoreActivity() {
        return nativeBrain.call("brain_active_non_core") instanceof String name ? Optional.ofNullable(activityOf(name)) : Optional.empty();
    }

    public void setActiveActivityIfPossible(Activity activity) {
        nativeBrain.call("brain_set_activity", nameOf(activity), "if_possible");
    }

    private void setActiveActivity(Activity activity) {
        nativeBrain.call("brain_set_activity", nameOf(activity), "force");
    }

    public void updateActivityFromSchedule(EnvironmentAttributeSystem environmentAttributes, long gameTime, Vec3 pos) {
        if (schedule == net.minecraft.world.attribute.EnvironmentAttributes.VILLAGER_ACTIVITY) {
            nativeBrain.call("brain_tick");
            return;
        }
        if (gameTime - lastScheduleUpdate > 20L) {
            lastScheduleUpdate = gameTime;
            Activity scheduled = schedule != null ? environmentAttributes.getValue(schedule, pos) : Activity.IDLE;
            if (!isActive(scheduled)) {
                setActiveActivityIfPossible(scheduled);
            }
        }
    }

    public void setActiveActivityToFirstValid(List<Activity> activities) {
        nativeBrain.call("brain_first_valid", (Object) activities.stream().map(Brain::nameOf).toArray(String[]::new));
    }

    public void setDefaultActivity(Activity activity) {
        defaultActivity = activity;
        nativeBrain.call("brain_set_default", nameOf(activity));
    }

    public void addActivity(Activity activity, ImmutableList<? extends Pair<Integer, ? extends BehaviorControl<? super E>>> behaviorPriorityPairs,
        Set<Pair<MemoryModuleType<?>, MemoryStatus>> conditions, Set<MemoryModuleType<?>> memoriesToEraseWhenStopped) {
        activitiesByName.put(nameOf(activity), activity);
        List<String> requirements = new ArrayList<>();
        for (Pair<MemoryModuleType<?>, MemoryStatus> condition : conditions) {
            registerMemory(condition.getFirst());
            requirements.add(keyOf(condition.getFirst()) + "=" + switch (condition.getSecond()) {
                case VALUE_PRESENT -> "present";
                case VALUE_ABSENT -> "absent";
                default -> "registered";
            });
        }
        nativeBrain.call("brain_define_activity", nameOf(activity), requirements.toArray(new String[0]),
            memoriesToEraseWhenStopped.stream().map(Brain::keyOf).toArray(String[]::new));
        for (Pair<Integer, ? extends BehaviorControl<? super E>> pair : behaviorPriorityPairs) {
            BehaviorControl<? super E> behavior = pair.getSecond();
            for (MemoryModuleType<?> required : behavior.getRequiredMemories()) {
                registerMemory(required);
            }
            availableBehaviorsByPriority.computeIfAbsent(pair.getFirst(), key -> new HashMap<>())
                .computeIfAbsent(activity, key -> new LinkedHashSet<>()).add(behavior);
            nativeBrain.call("brain_add_behavior", nameOf(activity), pair.getFirst().longValue(), NativeBehaviors.INSTANCE.bind(body, behavior));
        }
    }

    public void removeAllBehaviors() {
        for (Map<Activity, Set<BehaviorControl<? super E>>> byActivity : availableBehaviorsByPriority.values()) {
            byActivity.values().forEach(set -> set.forEach(behavior -> NativeBehaviors.INSTANCE.unbind(body, behavior)));
        }
        availableBehaviorsByPriority.clear();
        nativeBrain.call("brain_clear_behaviors");
    }

    public boolean isActive(Activity activity) {
        return nativeBrain.test("brain_is_active", nameOf(activity));
    }

    public void tick(ServerLevel level, E body) {
        nativeBrain.call("brain_tick");
        for (Sensor<? super E> sensor : sensors.values()) {
            sensor.tick(level, body);
        }
    }

    public void stopAll(ServerLevel level, E body) {
        long timestamp = body.level().getGameTime();
        for (BehaviorControl<? super E> behavior : getRunningBehaviors()) {
            behavior.doStop(level, body, timestamp);
        }
    }

    public boolean isBrainDead() {
        return registered.isEmpty() && sensors.isEmpty() && availableBehaviorsByPriority.isEmpty();
    }

    public static final class Provider<E extends LivingEntity> {
        private final Collection<? extends MemoryModuleType<?>> memoryTypes;
        private final Collection<? extends SensorType<? extends Sensor<? super E>>> sensorTypes;
        private final ActivitySupplier<E> activities;

        private Provider(Collection<? extends MemoryModuleType<?>> memoryTypes,
            Collection<? extends SensorType<? extends Sensor<? super E>>> sensorTypes, ActivitySupplier<E> activities) {
            this.memoryTypes = memoryTypes;
            this.sensorTypes = sensorTypes;
            this.activities = activities;
        }

        public Brain<E> makeBrain(E body, Packed packed) {
            return new Brain<>(body, memoryTypes, sensorTypes, activities.createActivities(body), packed.memories(), body.getRandom());
        }
    }

    public static record Packed(MemoryMap memories) {
        public static final Packed EMPTY = new Packed(MemoryMap.EMPTY);
        public static final Codec<Packed> CODEC = Codec.lazyInitialized(() -> RecordCodecBuilder.create(
            instance -> instance.group(MemoryMap.CODEC.fieldOf("memories").forGetter(Packed::memories)).apply(instance, Packed::new)));
    }

    @FunctionalInterface
    public interface ActivitySupplier<E extends LivingEntity> {
        List<ActivityData<E>> createActivities(E body);
    }

    public interface Visitor {
        <U> void acceptEmpty(MemoryModuleType<U> type);

        <U> void accept(MemoryModuleType<U> type, U value);

        <U> void accept(MemoryModuleType<U> type, U value, long timeToLive);
    }
}
