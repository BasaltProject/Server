// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.animal.allay;

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
import java.util.UUID;
import net.minecraft.core.BlockPos;
import net.minecraft.core.GlobalPos;
import net.minecraft.server.level.ServerPlayer;
import net.minecraft.world.entity.ai.Brain;
import net.minecraft.world.item.ItemStack;
import net.minecraft.world.level.Level;
import net.minecraft.world.level.block.Blocks;
import net.minecraft.world.phys.Vec3;

public class AllayAi {
    private static final float SPEED_MULTIPLIER_WHEN_IDLING = 1.0F;
    private static final float SPEED_MULTIPLIER_WHEN_FOLLOWING_DEPOSIT_TARGET = 2.25F;
    private static final float SPEED_MULTIPLIER_WHEN_RETRIEVING_ITEM = 1.75F;
    private static final float SPEED_MULTIPLIER_WHEN_PANICKING = 2.5F;
    private static final int CLOSE_ENOUGH_TO_TARGET = 4;
    private static final int TOO_FAR_FROM_TARGET = 16;
    private static final int MAX_LOOK_DISTANCE = 6;
    private static final int TIME_TO_FORGET_NOTEBLOCK = 600;
    private static final int DISTANCE_TO_WANTED_ITEM = 32;
    private static final int GIVE_ITEM_TIMEOUT_DURATION = 20;
    private static final Vec3 THROW_VELOCITY = new Vec3(0.2, 0.3, 0.2);
    private static final int ITEM_PICKUP_COOLDOWN_DURATION = 60;

    protected static List<ActivityData<Allay>> getActivities() {
        return List.of(coreActivity(), idleActivity());
    }

    private static ActivityData<Allay> coreActivity() {
        return ActivityData.<Allay>create(Activity.CORE, 0, ImmutableList.<BehaviorControl<? super Allay>>of(new Swim(0.8F), new AnimalPanic<>(SPEED_MULTIPLIER_WHEN_PANICKING),
            new LookAtTargetSink(45, 90), new MoveToTargetSink(), new CountDownCooldownTicks(MemoryModuleType.LIKED_NOTEBLOCK_COOLDOWN_TICKS),
            new CountDownCooldownTicks(MemoryModuleType.ITEM_PICKUP_COOLDOWN_TICKS)));
    }

    private static ActivityData<Allay> idleActivity() {
        return ActivityData.<Allay>create(Activity.IDLE, 0, ImmutableList.<BehaviorControl<? super Allay>>of(
            GoToWantedItem.<Allay>create(mob -> true, SPEED_MULTIPLIER_WHEN_RETRIEVING_ITEM, true, DISTANCE_TO_WANTED_ITEM),
            new GoAndGiveItemsToTarget<Allay>(AllayAi::getItemDepositPosition, SPEED_MULTIPLIER_WHEN_FOLLOWING_DEPOSIT_TARGET, GIVE_ITEM_TIMEOUT_DURATION,
                AllayAi::throwItem, MemoryModuleType.ITEM_PICKUP_COOLDOWN_TICKS, ITEM_PICKUP_COOLDOWN_DURATION, AllayAi::hasItemToThrow),
            StayCloseToTarget.create(AllayAi::getItemDepositPosition, Predicate.not(AllayAi::hasWantedItem), CLOSE_ENOUGH_TO_TARGET, TOO_FAR_FROM_TARGET,
                SPEED_MULTIPLIER_WHEN_FOLLOWING_DEPOSIT_TARGET),
            SetEntityLookTargetSometimes.create(MAX_LOOK_DISTANCE, UniformInt.of(30, 60)),
            new RunOne<Allay>(ImmutableList.of(Pair.of(RandomStroll.fly(SPEED_MULTIPLIER_WHEN_IDLING), 2),
                Pair.of(SetWalkTargetFromLookTarget.create(SPEED_MULTIPLIER_WHEN_IDLING, 3), 2), Pair.of(new DoNothing(30, 60), 1)))));
    }

    private static boolean hasItemToThrow(Allay allay) {
        return !allay.getInventory().isEmpty();
    }

    public static void updateActivity(Allay body) {
        body.getBrain().setActiveActivityToFirstValid(List.of(Activity.IDLE));
    }

    public static void hearNoteblock(LivingEntity allay, BlockPos pos) {
        Brain<?> brain = allay.getBrain();
        GlobalPos globalPos = GlobalPos.of(allay.level().dimension(), pos);
        Optional<GlobalPos> liked = brain.getMemory(MemoryModuleType.LIKED_NOTEBLOCK_POSITION);
        if (liked.isEmpty()) {
            brain.setMemory(MemoryModuleType.LIKED_NOTEBLOCK_POSITION, globalPos);
            brain.setMemory(MemoryModuleType.LIKED_NOTEBLOCK_COOLDOWN_TICKS, TIME_TO_FORGET_NOTEBLOCK);
        } else if (liked.get().equals(globalPos)) {
            brain.setMemory(MemoryModuleType.LIKED_NOTEBLOCK_COOLDOWN_TICKS, TIME_TO_FORGET_NOTEBLOCK);
        }
    }

    private static Optional<PositionTracker> getItemDepositPosition(LivingEntity allay) {
        Brain<?> brain = allay.getBrain();
        Optional<GlobalPos> liked = brain.getMemory(MemoryModuleType.LIKED_NOTEBLOCK_POSITION);
        if (liked.isPresent()) {
            GlobalPos position = liked.get();
            if (shouldDepositItemsAtLikedNoteblock(allay, brain, position)) {
                return Optional.of(new BlockPosTracker(position.pos().above()));
            }
            brain.eraseMemory(MemoryModuleType.LIKED_NOTEBLOCK_POSITION);
        }
        return getLikedPlayerPositionTracker(allay);
    }

    private static boolean hasWantedItem(LivingEntity allay) {
        return allay.getBrain().hasMemoryValue(MemoryModuleType.NEAREST_VISIBLE_WANTED_ITEM);
    }

    private static boolean shouldDepositItemsAtLikedNoteblock(LivingEntity allay, Brain<?> brain, GlobalPos likedNoteblockPos) {
        Optional<Integer> cooldown = brain.getMemory(MemoryModuleType.LIKED_NOTEBLOCK_COOLDOWN_TICKS);
        Level level = allay.level();
        return likedNoteblockPos.isCloseEnough(level.dimension(), allay.blockPosition(), 1024) && level.getBlockState(likedNoteblockPos.pos()).is(Blocks.NOTE_BLOCK)
            && cooldown.isPresent();
    }

    private static Optional<PositionTracker> getLikedPlayerPositionTracker(LivingEntity allay) {
        return getLikedPlayer(allay).map(player -> new EntityTracker(player, true));
    }

    public static Optional<ServerPlayer> getLikedPlayer(LivingEntity allay) {
        if (allay.level() instanceof ServerLevel serverLevel) {
            Optional<UUID> liked = allay.getBrain().getMemory(MemoryModuleType.LIKED_PLAYER);
            if (liked.isPresent() && serverLevel.getEntity(liked.get()) instanceof ServerPlayer player && player.closerThan(allay, 64.0)) {
                return Optional.of(player);
            }
        }
        return Optional.empty();
    }

    private static void throwItem(ServerLevel level, Allay thrower, Vec3 targetPos) {
        ItemStack item = thrower.getInventory().removeItem(0, 1);
        if (!item.isEmpty()) {
            BehaviorUtils.throwItem(thrower, item, targetPos.add(0.0, 1.0, 0.0), THROW_VELOCITY, 0.2F);
        }
    }
}
