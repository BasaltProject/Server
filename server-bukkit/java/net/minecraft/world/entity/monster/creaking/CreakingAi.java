// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.monster.creaking;

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
import net.minecraft.world.entity.player.Player;

public class CreakingAi {
    private static ActivityData<Creaking> coreActivity() {
        return ActivityData.<Creaking>create(Activity.CORE, 0, ImmutableList.<BehaviorControl<? super Creaking>>of(new Swim<Creaking>(0.8F) {
            protected boolean checkExtraStartConditions(ServerLevel level, Creaking body) {
                return body.canMove() && super.checkExtraStartConditions(level, body);
            }
        }, new LookAtTargetSink(45, 90), new MoveToTargetSink()));
    }

    private static ActivityData<Creaking> idleActivity() {
        return ActivityData.<Creaking>create(Activity.IDLE, 10, ImmutableList.<BehaviorControl<? super Creaking>>of(
            StartAttacking.<Creaking>create((level, mob) -> mob.isActive(), (level, mob) -> mob.getBrain().getMemory(MemoryModuleType.NEAREST_VISIBLE_ATTACKABLE_PLAYER)),
            SetEntityLookTargetSometimes.create(8.0F, UniformInt.of(30, 60)),
            new RunOne<Creaking>(ImmutableList.of(Pair.of(RandomStroll.stroll(0.3F), 2), Pair.of(SetWalkTargetFromLookTarget.create(0.3F, 3), 2),
                Pair.of(new DoNothing(30, 60), 1)))));
    }

    private static ActivityData<Creaking> fightActivity(Creaking body) {
        return ActivityData.<Creaking>create(Activity.FIGHT, ImmutableList.<Pair<Integer, ? extends BehaviorControl<? super Creaking>>>of(
            Pair.of(10, SetWalkTargetFromAttackTargetIfTargetOutOfReach.create(1.0F)), Pair.of(11, MeleeAttack.<Creaking>create(Creaking::canMove, 40)),
            Pair.of(12, StopAttackingIfTargetInvalid.<Creaking>create((level, target) -> !isAttackTargetStillReachable(body, target)))),
            Set.of(Pair.of(MemoryModuleType.ATTACK_TARGET, MemoryStatus.VALUE_PRESENT)));
    }

    private static boolean isAttackTargetStillReachable(Creaking creaking, LivingEntity target) {
        Optional<List<Player>> visible = creaking.getBrain().getMemory(MemoryModuleType.NEAREST_VISIBLE_ATTACKABLE_PLAYERS);
        return visible.map(players -> target instanceof Player player && players.contains(player)).orElse(false);
    }

    protected static List<ActivityData<Creaking>> getActivities(Creaking creaking) {
        return List.of(coreActivity(), idleActivity(), fightActivity(creaking));
    }

    public static void updateActivity(Creaking creaking) {
        if (!creaking.canMove()) {
            creaking.getBrain().useDefaultActivity();
        } else {
            creaking.getBrain().setActiveActivityToFirstValid(List.of(Activity.FIGHT, Activity.IDLE));
        }
    }
}
