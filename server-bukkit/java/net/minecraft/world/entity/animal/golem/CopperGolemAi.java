// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.animal.golem;

import com.google.common.collect.ImmutableList;
import com.mojang.datafixers.util.Pair;
import java.util.List;
import java.util.Map;
import java.util.function.Consumer;
import java.util.function.Predicate;
import net.minecraft.sounds.SoundEvent;
import net.minecraft.sounds.SoundEvents;
import net.minecraft.tags.BlockTags;
import net.minecraft.util.valueproviders.UniformInt;
import net.minecraft.world.Container;
import net.minecraft.world.entity.EntityTypes;
import net.minecraft.world.entity.PathfinderMob;
import net.minecraft.world.entity.ai.ActivityData;
import net.minecraft.world.entity.ai.behavior.AnimalPanic;
import net.minecraft.world.entity.ai.behavior.BehaviorControl;
import net.minecraft.world.entity.ai.behavior.CountDownCooldownTicks;
import net.minecraft.world.entity.ai.behavior.DoNothing;
import net.minecraft.world.entity.ai.behavior.InteractWithDoor;
import net.minecraft.world.entity.ai.behavior.LookAtTargetSink;
import net.minecraft.world.entity.ai.behavior.MoveToTargetSink;
import net.minecraft.world.entity.ai.behavior.RandomStroll;
import net.minecraft.world.entity.ai.behavior.RunOne;
import net.minecraft.world.entity.ai.behavior.SetEntityLookTargetSometimes;
import net.minecraft.world.entity.ai.behavior.TransportItemsBetweenContainers;
import net.minecraft.world.entity.ai.memory.MemoryModuleType;
import net.minecraft.world.entity.ai.memory.MemoryStatus;
import net.minecraft.world.entity.schedule.Activity;
import net.minecraft.world.level.block.Blocks;
import net.minecraft.world.level.block.entity.ChestBlockEntity;
import net.minecraft.world.level.block.state.BlockState;

public class CopperGolemAi {
    private static final float SPEED_MULTIPLIER_WHEN_PANICKING = 1.5F;
    private static final float SPEED_MULTIPLIER_WHEN_IDLING = 1.0F;
    private static final int TRANSPORT_ITEM_HORIZONTAL_SEARCH_RADIUS = 32;
    private static final int TRANSPORT_ITEM_VERTICAL_SEARCH_RADIUS = 8;
    private static final int TICK_TO_START_ON_REACHED_INTERACTION = 1;
    private static final int TICK_TO_PLAY_ON_REACHED_SOUND = 9;
    private static final Predicate<BlockState> TRANSPORT_ITEM_SOURCE_BLOCK = block -> block.is(BlockTags.COPPER_CHESTS);
    private static final Predicate<BlockState> TRANSPORT_ITEM_DESTINATION_BLOCK = block -> block.is(Blocks.CHEST) || block.is(Blocks.TRAPPED_CHEST);

    protected static List<ActivityData<CopperGolem>> getActivities() {
        return List.of(coreActivity(), idleActivity());
    }

    public static void updateActivity(CopperGolem body) {
        body.getBrain().setActiveActivityToFirstValid(List.of(Activity.IDLE));
    }

    private static ActivityData<CopperGolem> coreActivity() {
        return ActivityData.<CopperGolem>create(Activity.CORE, 0, ImmutableList.<BehaviorControl<? super CopperGolem>>of(new AnimalPanic<>(SPEED_MULTIPLIER_WHEN_PANICKING),
            new LookAtTargetSink(45, 90), new MoveToTargetSink(), InteractWithDoor.create(), new CountDownCooldownTicks(MemoryModuleType.GAZE_COOLDOWN_TICKS),
            new CountDownCooldownTicks(MemoryModuleType.TRANSPORT_ITEMS_COOLDOWN_TICKS)));
    }

    private static ActivityData<CopperGolem> idleActivity() {
        return ActivityData.<CopperGolem>create(Activity.IDLE, ImmutableList.<Pair<Integer, ? extends BehaviorControl<? super CopperGolem>>>of(
            Pair.of(0, new TransportItemsBetweenContainers(SPEED_MULTIPLIER_WHEN_IDLING, TRANSPORT_ITEM_SOURCE_BLOCK, TRANSPORT_ITEM_DESTINATION_BLOCK,
                TRANSPORT_ITEM_HORIZONTAL_SEARCH_RADIUS, TRANSPORT_ITEM_VERTICAL_SEARCH_RADIUS, targetReachedInteractions(), onTravelling(), shouldQueueForTarget())),
            Pair.of(1, SetEntityLookTargetSometimes.create(EntityTypes.PLAYER, 6.0F, UniformInt.of(40, 80))),
            Pair.of(2, new RunOne<CopperGolem>(Map.of(MemoryModuleType.WALK_TARGET, MemoryStatus.VALUE_ABSENT, MemoryModuleType.TRANSPORT_ITEMS_COOLDOWN_TICKS, MemoryStatus.VALUE_PRESENT),
                ImmutableList.of(Pair.of(RandomStroll.stroll(SPEED_MULTIPLIER_WHEN_IDLING, 2, 2), 1), Pair.of(new DoNothing(30, 60), 1))))));
    }

    private static Map<TransportItemsBetweenContainers.ContainerInteractionState, TransportItemsBetweenContainers.OnTargetReachedInteraction> targetReachedInteractions() {
        return Map.of(
            TransportItemsBetweenContainers.ContainerInteractionState.PICKUP_ITEM, onReachedTargetInteraction(CopperGolemState.GETTING_ITEM, SoundEvents.COPPER_GOLEM_ITEM_GET),
            TransportItemsBetweenContainers.ContainerInteractionState.PICKUP_NO_ITEM, onReachedTargetInteraction(CopperGolemState.GETTING_NO_ITEM, SoundEvents.COPPER_GOLEM_ITEM_NO_GET),
            TransportItemsBetweenContainers.ContainerInteractionState.PLACE_ITEM, onReachedTargetInteraction(CopperGolemState.DROPPING_ITEM, SoundEvents.COPPER_GOLEM_ITEM_DROP),
            TransportItemsBetweenContainers.ContainerInteractionState.PLACE_NO_ITEM, onReachedTargetInteraction(CopperGolemState.DROPPING_NO_ITEM, SoundEvents.COPPER_GOLEM_ITEM_NO_DROP));
    }

    private static TransportItemsBetweenContainers.OnTargetReachedInteraction onReachedTargetInteraction(CopperGolemState state, SoundEvent sound) {
        return (body, target, ticks) -> {
            if (body instanceof CopperGolem golem) {
                Container container = target.container();
                if (ticks == TICK_TO_START_ON_REACHED_INTERACTION) {
                    container.startOpen(golem);
                    golem.setOpenedChestPos(target.pos());
                    golem.setState(state);
                }
                if (ticks == TICK_TO_PLAY_ON_REACHED_SOUND && sound != null) {
                    golem.playSound(sound);
                }
                if (ticks == TransportItemsBetweenContainers.TARGET_INTERACTION_TIME) {
                    if (container.getEntitiesWithContainerOpen().contains(body)) {
                        container.stopOpen(golem);
                    }
                    golem.clearOpenedChestPos();
                }
            }
        };
    }

    private static Consumer<PathfinderMob> onTravelling() {
        return body -> {
            if (body instanceof CopperGolem golem) {
                golem.clearOpenedChestPos();
                golem.setState(CopperGolemState.IDLE);
            }
        };
    }

    private static Predicate<TransportItemsBetweenContainers.TransportItemTarget> shouldQueueForTarget() {
        return target -> target.blockEntity() instanceof ChestBlockEntity chest && !chest.getEntitiesWithContainerOpen().isEmpty();
    }
}
