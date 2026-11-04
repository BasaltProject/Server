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
import net.minecraft.world.entity.ai.memory.MemorySlot;
import net.minecraft.world.entity.ai.memory.MemoryStatus;
import net.minecraft.world.entity.ai.sensing.Sensor;
import net.minecraft.world.entity.ai.sensing.SensorType;
import net.minecraft.world.entity.schedule.Activity;
import net.minecraft.world.phys.Vec3;

public class Brain<E extends LivingEntity> {
    private static final int kScheduleUpdateDelay = 20;

    private final Map<MemoryModuleType<?>, MemorySlot<?>> memories = new HashMap<>();
    private final Map<SensorType<? extends Sensor<? super E>>, Sensor<? super E>> sensors = new LinkedHashMap<>();
    private final Map<Integer, Map<Activity, Set<BehaviorControl<? super E>>>> availableBehaviorsByPriority = new TreeMap<>();
    private EnvironmentAttribute<Activity> schedule;
    private final Map<Activity, Set<Pair<MemoryModuleType<?>, MemoryStatus>>> activityRequirements = new HashMap<>();
    private final Map<Activity, Set<MemoryModuleType<?>>> activityMemoriesToEraseWhenStopped = new HashMap<>();
    private Set<Activity> coreActivities = new HashSet<>();
    private final Set<Activity> activeActivities = new HashSet<>();
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

    protected Brain(Collection<? extends MemoryModuleType<?>> memoryTypes, Collection<? extends SensorType<? extends Sensor<? super E>>> sensorTypes,
        List<ActivityData<E>> activities, MemoryMap memories, RandomSource randomSource) {
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

    public Brain() {
        setCoreActivities(ImmutableSet.of(Activity.CORE));
        useDefaultActivity();
    }

    private void registerMemory(MemoryModuleType<?> memoryType) {
        memories.putIfAbsent(memoryType, MemorySlot.create());
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

    @SuppressWarnings("unchecked")
    private <T> MemorySlot<T> slotIfPresent(MemoryModuleType<T> memoryType) {
        return (MemorySlot<T>) memories.get(memoryType);
    }

    private <T> MemorySlot<T> slot(MemoryModuleType<T> memoryType) {
        MemorySlot<T> result = slotIfPresent(memoryType);
        if (result == null) {
            throw new IllegalStateException("Unregistered memory fetched: " + memoryType);
        }
        return result;
    }

    public boolean hasMemoryValue(MemoryModuleType<?> type) {
        return checkMemory(type, MemoryStatus.VALUE_PRESENT);
    }

    public void clearMemories() {
        memories.values().forEach(MemorySlot::clear);
    }

    public <U> void eraseMemory(MemoryModuleType<U> type) {
        MemorySlot<U> slot = slotIfPresent(type);
        if (slot != null) {
            slot.clear();
        }
    }

    public <U> void setMemory(MemoryModuleType<U> type, U value) {
        setMemoryInternal(type, value);
    }

    public <U> void setMemoryWithExpiry(MemoryModuleType<U> type, U value, long timeToLive) {
        setMemoryInternal(type, value, timeToLive);
    }

    public <U> void setMemory(MemoryModuleType<U> type, Optional<? extends U> optionalValue) {
        setMemoryInternal(type, optionalValue.orElse(null));
    }

    private <U> void setMemoryInternal(MemoryMap.Value<U> value) {
        ExpirableValue<U> expirable = value.value();
        if (expirable.timeToLive().isPresent()) {
            setMemoryInternal(value.type(), expirable.value(), expirable.timeToLive().get());
        } else {
            setMemoryInternal(value.type(), expirable.value());
        }
    }

    private static boolean isEmptyCollection(Object value) {
        return value instanceof Collection<?> collection && collection.isEmpty();
    }

    private <U> void setMemoryInternal(MemoryModuleType<U> type, U value, long timeToLive) {
        MemorySlot<U> slot = slotIfPresent(type);
        if (slot != null) {
            if (value == null || isEmptyCollection(value)) {
                slot.clear();
            } else {
                slot.set(value, timeToLive);
            }
        }
    }

    private <U> void setMemoryInternal(MemoryModuleType<U> type, U value) {
        MemorySlot<U> slot = slotIfPresent(type);
        if (slot != null) {
            if (value == null || isEmptyCollection(value)) {
                slot.clear();
            } else {
                slot.set(value);
            }
        }
    }

    public <U> Optional<U> getMemory(MemoryModuleType<U> type) {
        return Optional.ofNullable(slot(type).value());
    }

    public <U> Optional<U> getMemoryInternal(MemoryModuleType<U> type) {
        MemorySlot<U> slot = slotIfPresent(type);
        return slot == null ? null : Optional.ofNullable(slot.value());
    }

    public <U> long getTimeUntilExpiry(MemoryModuleType<U> type) {
        return slot(type).timeToLive();
    }

    public void forEach(Visitor visitor) {
        memories.forEach((type, slot) -> visit(visitor, type, slot));
    }

    @SuppressWarnings("unchecked")
    private static <U> void visit(Visitor visitor, MemoryModuleType<U> type, MemorySlot<?> slot) {
        ((MemorySlot<U>) slot).visit(type, visitor);
    }

    public <U> boolean isMemoryValue(MemoryModuleType<U> memoryType, U value) {
        MemorySlot<U> slot = slotIfPresent(memoryType);
        return slot != null && Objects.equals(value, slot.value());
    }

    public boolean checkMemory(MemoryModuleType<?> type, MemoryStatus status) {
        MemorySlot<?> slot = memories.get(type);
        if (slot == null) {
            return false;
        }
        return status == MemoryStatus.REGISTERED || (status == MemoryStatus.VALUE_PRESENT && slot.hasValue())
            || (status == MemoryStatus.VALUE_ABSENT && !slot.hasValue());
    }

    public void setSchedule(EnvironmentAttribute<Activity> schedule) {
        this.schedule = schedule;
    }

    public void setCoreActivities(Set<Activity> activities) {
        coreActivities = activities;
    }

    @Deprecated
    public Set<Activity> getActiveActivities() {
        return activeActivities;
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
        setActiveActivity(defaultActivity);
    }

    public Optional<Activity> getActiveNonCoreActivity() {
        for (Activity activity : activeActivities) {
            if (!coreActivities.contains(activity)) {
                return Optional.of(activity);
            }
        }
        return Optional.empty();
    }

    public void setActiveActivityIfPossible(Activity activity) {
        if (activityRequirementsAreMet(activity)) {
            setActiveActivity(activity);
        } else {
            useDefaultActivity();
        }
    }

    private void setActiveActivity(Activity activity) {
        if (!isActive(activity)) {
            eraseMemoriesForOtherActivitiesThan(activity);
            activeActivities.clear();
            activeActivities.addAll(coreActivities);
            activeActivities.add(activity);
        }
    }

    private void eraseMemoriesForOtherActivitiesThan(Activity activity) {
        for (Activity old : activeActivities) {
            if (old != activity) {
                Set<MemoryModuleType<?>> toErase = activityMemoriesToEraseWhenStopped.get(old);
                if (toErase != null) {
                    toErase.forEach(this::eraseMemory);
                }
            }
        }
    }

    public void updateActivityFromSchedule(EnvironmentAttributeSystem environmentAttributes, long gameTime, Vec3 pos) {
        if (gameTime - lastScheduleUpdate > kScheduleUpdateDelay) {
            lastScheduleUpdate = gameTime;
            Activity scheduled = schedule != null ? environmentAttributes.getValue(schedule, pos) : Activity.IDLE;
            if (!activeActivities.contains(scheduled)) {
                setActiveActivityIfPossible(scheduled);
            }
        }
    }

    public void setActiveActivityToFirstValid(List<Activity> activities) {
        for (Activity activity : activities) {
            if (activityRequirementsAreMet(activity)) {
                setActiveActivity(activity);
                break;
            }
        }
    }

    public void setDefaultActivity(Activity activity) {
        defaultActivity = activity;
    }

    public void addActivity(Activity activity, ImmutableList<? extends Pair<Integer, ? extends BehaviorControl<? super E>>> behaviorPriorityPairs,
        Set<Pair<MemoryModuleType<?>, MemoryStatus>> conditions, Set<MemoryModuleType<?>> memoriesToEraseWhenStopped) {
        activityRequirements.put(activity, conditions);
        if (!memoriesToEraseWhenStopped.isEmpty()) {
            activityMemoriesToEraseWhenStopped.put(activity, memoriesToEraseWhenStopped);
        }
        for (Pair<Integer, ? extends BehaviorControl<? super E>> pair : behaviorPriorityPairs) {
            BehaviorControl<? super E> behavior = pair.getSecond();
            for (MemoryModuleType<?> required : behavior.getRequiredMemories()) {
                registerMemory(required);
            }
            availableBehaviorsByPriority.computeIfAbsent(pair.getFirst(), key -> new HashMap<>())
                .computeIfAbsent(activity, key -> new LinkedHashSet<>()).add(behavior);
        }
    }

    public void removeAllBehaviors() {
        availableBehaviorsByPriority.clear();
    }

    public boolean isActive(Activity activity) {
        return activeActivities.contains(activity);
    }

    public void tick(ServerLevel level, E body) {
        memories.values().forEach(MemorySlot::tick);
        for (Sensor<? super E> sensor : sensors.values()) {
            sensor.tick(level, body);
        }
        long time = level.getGameTime();
        for (Map<Activity, Set<BehaviorControl<? super E>>> byActivity : availableBehaviorsByPriority.values()) {
            for (Map.Entry<Activity, Set<BehaviorControl<? super E>>> forActivity : byActivity.entrySet()) {
                if (activeActivities.contains(forActivity.getKey())) {
                    for (BehaviorControl<? super E> behavior : forActivity.getValue()) {
                        if (behavior.getStatus() == Behavior.Status.STOPPED) {
                            behavior.tryStart(level, body, time);
                        }
                    }
                }
            }
        }
        for (BehaviorControl<? super E> behavior : getRunningBehaviors()) {
            behavior.tickOrStop(level, body, time);
        }
    }

    public void stopAll(ServerLevel level, E body) {
        long timestamp = body.level().getGameTime();
        for (BehaviorControl<? super E> behavior : getRunningBehaviors()) {
            behavior.doStop(level, body, timestamp);
        }
    }

    private boolean activityRequirementsAreMet(Activity activity) {
        Set<Pair<MemoryModuleType<?>, MemoryStatus>> requirements = activityRequirements.get(activity);
        if (requirements == null) {
            return false;
        }
        for (Pair<MemoryModuleType<?>, MemoryStatus> requirement : requirements) {
            if (!checkMemory(requirement.getFirst(), requirement.getSecond())) {
                return false;
            }
        }
        return true;
    }

    public boolean isBrainDead() {
        return memories.isEmpty() && sensors.isEmpty() && availableBehaviorsByPriority.isEmpty();
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
            return new Brain<>(memoryTypes, sensorTypes, activities.createActivities(body), packed.memories(), body.getRandom());
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
