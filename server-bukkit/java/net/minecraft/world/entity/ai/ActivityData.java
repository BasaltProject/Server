// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.ai;

import com.google.common.collect.ImmutableList;
import com.mojang.datafixers.util.Pair;
import java.util.Set;
import net.minecraft.world.entity.LivingEntity;
import net.minecraft.world.entity.ai.behavior.BehaviorControl;
import net.minecraft.world.entity.ai.memory.MemoryModuleType;
import net.minecraft.world.entity.ai.memory.MemoryStatus;
import net.minecraft.world.entity.schedule.Activity;

public record ActivityData<E extends LivingEntity>(Activity activityType,
    ImmutableList<? extends Pair<Integer, ? extends BehaviorControl<? super E>>> behaviorPriorityPairs,
    Set<Pair<MemoryModuleType<?>, MemoryStatus>> conditions, Set<MemoryModuleType<?>> memoriesToEraseWhenStopped) {
    /** An activity with no Java behaviors: the native brain of the species runs it. */
    public static <E extends LivingEntity> ActivityData<E> nativeOnly(Activity activity) {
        return create(activity, ImmutableList.of());
    }

    public static <E extends LivingEntity> ActivityData<E> create(Activity activity, int priorityOfFirstBehavior,
        ImmutableList<? extends BehaviorControl<? super E>> behaviorList) {
        return create(activity, createPriorityPairs(priorityOfFirstBehavior, behaviorList));
    }

    public static <E extends LivingEntity> ActivityData<E> create(Activity activity, int priorityOfFirstBehavior,
        ImmutableList<? extends BehaviorControl<? super E>> behaviorList, MemoryModuleType<?> memoryThatMustHaveValueAndWillBeErasedAfter) {
        return create(activity, createPriorityPairs(priorityOfFirstBehavior, behaviorList),
            Set.of(Pair.of(memoryThatMustHaveValueAndWillBeErasedAfter, MemoryStatus.VALUE_PRESENT)),
            Set.of(memoryThatMustHaveValueAndWillBeErasedAfter));
    }

    public static <E extends LivingEntity> ActivityData<E> create(Activity activity,
        ImmutableList<? extends Pair<Integer, ? extends BehaviorControl<? super E>>> behaviorPriorityPairs) {
        return create(activity, behaviorPriorityPairs, Set.of());
    }

    public static <E extends LivingEntity> ActivityData<E> create(Activity activity, int priorityOfFirstBehavior,
        ImmutableList<? extends BehaviorControl<? super E>> behaviorList, Set<Pair<MemoryModuleType<?>, MemoryStatus>> conditions) {
        return create(activity, createPriorityPairs(priorityOfFirstBehavior, behaviorList), conditions);
    }

    public static <E extends LivingEntity> ActivityData<E> create(Activity activity,
        ImmutableList<? extends Pair<Integer, ? extends BehaviorControl<? super E>>> behaviorPriorityPairs,
        Set<Pair<MemoryModuleType<?>, MemoryStatus>> conditions) {
        return create(activity, behaviorPriorityPairs, conditions, Set.of());
    }

    public static <E extends LivingEntity> ActivityData<E> create(Activity activity,
        ImmutableList<? extends Pair<Integer, ? extends BehaviorControl<? super E>>> behaviorPriorityPairs,
        Set<Pair<MemoryModuleType<?>, MemoryStatus>> conditions, Set<MemoryModuleType<?>> memoriesToEraseWhenStopped) {
        return new ActivityData<>(activity, behaviorPriorityPairs, conditions, memoriesToEraseWhenStopped);
    }

    public static <E extends LivingEntity> ImmutableList<? extends Pair<Integer, ? extends BehaviorControl<? super E>>> createPriorityPairs(
        int priorityOfFirstBehavior, ImmutableList<? extends BehaviorControl<? super E>> behaviorList) {
        int priority = priorityOfFirstBehavior;
        ImmutableList.Builder<Pair<Integer, ? extends BehaviorControl<? super E>>> builder = ImmutableList.builder();
        for (BehaviorControl<? super E> behavior : behaviorList) {
            builder.add(Pair.of(priority++, behavior));
        }
        return builder.build();
    }
}
