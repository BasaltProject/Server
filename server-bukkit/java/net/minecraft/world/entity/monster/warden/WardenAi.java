// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.monster.warden;

import com.google.common.collect.ImmutableList;
import com.mojang.datafixers.util.Pair;
import java.util.List;
import java.util.Set;
import java.util.Map;
import java.util.Optional;
import java.util.function.Predicate;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.util.RandomSource;
import net.minecraft.util.valueproviders.UniformInt;
import net.minecraft.world.entity.*;
import net.minecraft.world.entity.ai.ActivityData;
import net.minecraft.world.entity.ai.behavior.*;
import net.minecraft.world.entity.ai.behavior.declarative.BehaviorBuilder;
import net.minecraft.world.entity.ai.memory.MemoryModuleType;
import net.minecraft.world.entity.ai.memory.MemoryStatus;
import net.minecraft.world.entity.schedule.Activity;
import net.minecraft.util.Unit;
import net.minecraft.core.BlockPos;
import net.minecraft.util.Mth;
import net.minecraft.world.entity.ai.Brain;
import net.minecraft.world.entity.ai.attributes.Attributes;
import net.minecraft.world.entity.ai.behavior.warden.Digging;
import net.minecraft.world.entity.ai.behavior.warden.Emerging;
import net.minecraft.world.entity.ai.behavior.warden.ForceUnmount;
import net.minecraft.world.entity.ai.behavior.warden.Roar;
import net.minecraft.world.entity.ai.behavior.warden.SetRoarTarget;
import net.minecraft.world.entity.ai.behavior.warden.SetWardenLookTarget;
import net.minecraft.world.entity.ai.behavior.warden.Sniffing;
import net.minecraft.world.entity.ai.behavior.warden.SonicBoom;
import net.minecraft.world.entity.ai.behavior.warden.TryToSniff;

public class WardenAi {
    private static final float SPEED_MULTIPLIER_WHEN_IDLING = 0.5F;
    private static final float SPEED_MULTIPLIER_WHEN_INVESTIGATING = 0.7F;
    private static final float SPEED_MULTIPLIER_WHEN_FIGHTING = 1.2F;
    private static final int MELEE_ATTACK_COOLDOWN = 18;
    private static final int DIGGING_DURATION = Mth.ceil(100.0F);
    public static final int EMERGE_DURATION = Mth.ceil(133.59999F);
    public static final int ROAR_DURATION = Mth.ceil(84.0F);
    private static final int SNIFFING_DURATION = Mth.ceil(83.2F);
    public static final int DIGGING_COOLDOWN = 1200;
    private static final int DISTURBANCE_LOCATION_EXPIRY_TIME = 100;
    private static final BehaviorControl<Warden> DIG_COOLDOWN_SETTER = OneShots.<Warden>of(Set.of(MemoryModuleType.DIG_COOLDOWN), (level, body, timestamp) -> {
        if (body.getBrain().hasMemoryValue(MemoryModuleType.DIG_COOLDOWN)) {
            body.getBrain().setMemoryWithExpiry(MemoryModuleType.DIG_COOLDOWN, Unit.INSTANCE, DIGGING_COOLDOWN);
        }
        return true;
    });

    protected static List<ActivityData<Warden>> getActivities(Warden body) {
        return List.of(coreActivity(), emergeActivity(), diggingActivity(), idleActivity(), roarActivity(), fightActivity(body), investigateActivity(), sniffingActivity());
    }

    public static void updateActivity(Brain<Warden> brain) {
        brain.setActiveActivityToFirstValid(List.of(Activity.EMERGE, Activity.DIG, Activity.ROAR, Activity.FIGHT, Activity.INVESTIGATE, Activity.SNIFF, Activity.IDLE));
    }

    private static ActivityData<Warden> coreActivity() {
        return ActivityData.<Warden>create(Activity.CORE, 0, ImmutableList.<BehaviorControl<? super Warden>>of(new Swim(0.8F), SetWardenLookTarget.create(),
            new LookAtTargetSink(45, 90), new MoveToTargetSink()));
    }

    private static ActivityData<Warden> emergeActivity() {
        return ActivityData.<Warden>create(Activity.EMERGE, 5, ImmutableList.<BehaviorControl<? super Warden>>of(new Emerging<Warden>(EMERGE_DURATION)), MemoryModuleType.IS_EMERGING);
    }

    private static ActivityData<Warden> diggingActivity() {
        return ActivityData.<Warden>create(Activity.DIG, ImmutableList.<Pair<Integer, ? extends BehaviorControl<? super Warden>>>of(Pair.of(0, new ForceUnmount()),
            Pair.of(1, new Digging<Warden>(DIGGING_DURATION))), Set.of(Pair.of(MemoryModuleType.ROAR_TARGET, MemoryStatus.VALUE_ABSENT),
                Pair.of(MemoryModuleType.DIG_COOLDOWN, MemoryStatus.VALUE_ABSENT)));
    }

    private static ActivityData<Warden> idleActivity() {
        return ActivityData.<Warden>create(Activity.IDLE, 10, ImmutableList.<BehaviorControl<? super Warden>>of(SetRoarTarget.<Warden>create(Warden::getEntityAngryAt), TryToSniff.create(),
            new RunOne<Warden>(Map.of(MemoryModuleType.IS_SNIFFING, MemoryStatus.VALUE_ABSENT), ImmutableList.of(Pair.of(RandomStroll.stroll(SPEED_MULTIPLIER_WHEN_IDLING), 2),
                Pair.of(new DoNothing(30, 60), 1)))));
    }

    private static ActivityData<Warden> investigateActivity() {
        return ActivityData.<Warden>create(Activity.INVESTIGATE, 5, ImmutableList.<BehaviorControl<? super Warden>>of(SetRoarTarget.<Warden>create(Warden::getEntityAngryAt),
            GoToTargetLocation.<Warden>create(MemoryModuleType.DISTURBANCE_LOCATION, 2, SPEED_MULTIPLIER_WHEN_INVESTIGATING)), MemoryModuleType.DISTURBANCE_LOCATION);
    }

    private static ActivityData<Warden> sniffingActivity() {
        return ActivityData.<Warden>create(Activity.SNIFF, 5, ImmutableList.<BehaviorControl<? super Warden>>of(SetRoarTarget.<Warden>create(Warden::getEntityAngryAt),
            new Sniffing<Warden>(SNIFFING_DURATION)), MemoryModuleType.IS_SNIFFING);
    }

    private static ActivityData<Warden> roarActivity() {
        return ActivityData.<Warden>create(Activity.ROAR, 10, ImmutableList.<BehaviorControl<? super Warden>>of(new Roar()), MemoryModuleType.ROAR_TARGET);
    }

    private static ActivityData<Warden> fightActivity(Warden body) {
        return ActivityData.<Warden>create(Activity.FIGHT, 10, ImmutableList.<BehaviorControl<? super Warden>>of(DIG_COOLDOWN_SETTER,
            StopAttackingIfTargetInvalid.<Warden>create((level, target) -> !body.getAngerLevel().isAngry() || !body.canTargetEntity(target), WardenAi::onTargetInvalid, false),
            SetEntityLookTarget.create(entity -> isTarget(body, entity), (float) body.getAttributeValue(Attributes.FOLLOW_RANGE)),
            SetWalkTargetFromAttackTargetIfTargetOutOfReach.create(SPEED_MULTIPLIER_WHEN_FIGHTING), new SonicBoom(), MeleeAttack.<Warden>create(MELEE_ATTACK_COOLDOWN)),
            MemoryModuleType.ATTACK_TARGET);
    }

    private static boolean isTarget(Warden body, LivingEntity living) {
        return body.getBrain().getMemory(MemoryModuleType.ATTACK_TARGET).filter(e -> e == living).isPresent();
    }

    private static void onTargetInvalid(ServerLevel level, Warden body, LivingEntity attackTarget) {
        if (!body.canTargetEntity(attackTarget)) {
            body.clearAnger(attackTarget);
        }
        setDigCooldown(body);
    }

    public static void setDigCooldown(LivingEntity body) {
        if (body.getBrain().hasMemoryValue(MemoryModuleType.DIG_COOLDOWN)) {
            body.getBrain().setMemoryWithExpiry(MemoryModuleType.DIG_COOLDOWN, Unit.INSTANCE, DIGGING_COOLDOWN);
        }
    }

    public static void setDisturbanceLocation(Warden body, BlockPos position) {
        if (body.level().getWorldBorder().isWithinBounds(position) && body.getEntityAngryAt().isEmpty() && body.getBrain().getMemory(MemoryModuleType.ATTACK_TARGET).isEmpty()) {
            setDigCooldown(body);
            body.getBrain().setMemoryWithExpiry(MemoryModuleType.SNIFF_COOLDOWN, Unit.INSTANCE, DISTURBANCE_LOCATION_EXPIRY_TIME);
            body.getBrain().setMemoryWithExpiry(MemoryModuleType.LOOK_TARGET, new BlockPosTracker(position), DISTURBANCE_LOCATION_EXPIRY_TIME);
            body.getBrain().setMemoryWithExpiry(MemoryModuleType.DISTURBANCE_LOCATION, position, DISTURBANCE_LOCATION_EXPIRY_TIME);
            body.getBrain().eraseMemory(MemoryModuleType.WALK_TARGET);
        }
    }
}
