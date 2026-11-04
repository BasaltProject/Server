// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.monster.piglin;

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
import net.minecraft.world.entity.ai.behavior.declarative.Trigger;
import java.util.ArrayList;
import java.util.Collections;
import net.minecraft.sounds.SoundEvent;
import net.minecraft.sounds.SoundEvents;
import net.minecraft.tags.ItemTags;
import net.minecraft.util.TimeUtil;
import net.minecraft.world.InteractionHand;
import net.minecraft.world.InteractionResult;
import net.minecraft.world.entity.ai.Brain;
import net.minecraft.world.entity.ai.sensing.Sensor;
import net.minecraft.world.entity.ai.util.LandRandomPos;
import net.minecraft.world.entity.item.ItemEntity;
import net.minecraft.world.entity.monster.hoglin.Hoglin;
import net.minecraft.world.entity.player.Player;
import net.minecraft.world.item.Item;
import net.minecraft.world.item.ItemStack;
import net.minecraft.world.item.Items;
import net.minecraft.world.level.gamerules.GameRules;
import net.minecraft.world.phys.Vec3;

public class PiglinAi {
    public static final int REPELLENT_DETECTION_RANGE_HORIZONTAL = 8;
    public static final int REPELLENT_DETECTION_RANGE_VERTICAL = 4;
    public static final Item BARTERING_ITEM = Items.GOLD_INGOT;
    private static final int PLAYER_ANGER_RANGE = 16;
    private static final int ANGER_DURATION = 600;
    private static final int ADMIRE_DURATION = 119;
    private static final int MAX_DISTANCE_TO_WALK_TO_ITEM = 9;
    private static final int MAX_TIME_TO_WALK_TO_ITEM = 200;
    private static final int HOW_LONG_TIME_TO_DISABLE_ADMIRE_WALKING_IF_CANT_REACH_ITEM = 200;
    private static final int CELEBRATION_TIME = 300;
    public static final int MAX_TIME_BETWEEN_HUNTS = 120;
    protected static final UniformInt TIME_BETWEEN_HUNTS = TimeUtil.rangeOfSeconds(30, 120);
    private static final int BABY_FLEE_DURATION_AFTER_GETTING_HIT = 100;
    private static final int HIT_BY_PLAYER_MEMORY_TIMEOUT = 400;
    private static final int MAX_WALK_DISTANCE_TO_START_RIDING = 8;
    private static final UniformInt RIDE_START_INTERVAL = TimeUtil.rangeOfSeconds(10, 40);
    private static final UniformInt RIDE_DURATION = TimeUtil.rangeOfSeconds(10, 30);
    private static final UniformInt RETREAT_DURATION = TimeUtil.rangeOfSeconds(5, 20);
    private static final int MELEE_ATTACK_COOLDOWN = 20;
    private static final int EAT_COOLDOWN = 200;
    private static final int DESIRED_DISTANCE_FROM_ENTITY_WHEN_AVOIDING = 12;
    private static final int MAX_LOOK_DIST = 8;
    private static final int MAX_LOOK_DIST_FOR_PLAYER_HOLDING_LOVED_ITEM = 14;
    private static final int INTERACTION_RANGE = 8;
    private static final int MIN_DESIRED_DIST_FROM_TARGET_WHEN_HOLDING_CROSSBOW = 5;
    private static final float SPEED_WHEN_STRAFING_BACK_FROM_TARGET = 0.75F;
    private static final int DESIRED_DISTANCE_FROM_ZOMBIFIED = 6;
    private static final UniformInt AVOID_ZOMBIFIED_DURATION = TimeUtil.rangeOfSeconds(5, 7);
    private static final UniformInt BABY_AVOID_NEMESIS_DURATION = TimeUtil.rangeOfSeconds(5, 7);
    private static final float PROBABILITY_OF_CELEBRATION_DANCE = 0.1F;
    private static final float SPEED_MULTIPLIER_WHEN_AVOIDING = 1.0F;
    private static final float SPEED_MULTIPLIER_WHEN_RETREATING = 1.0F;
    private static final float SPEED_MULTIPLIER_WHEN_MOUNTING = 0.8F;
    private static final float SPEED_MULTIPLIER_WHEN_GOING_TO_WANTED_ITEM = 1.0F;
    private static final float SPEED_MULTIPLIER_WHEN_GOING_TO_CELEBRATE_LOCATION = 1.0F;
    private static final float SPEED_MULTIPLIER_WHEN_DANCING = 0.6F;
    private static final float SPEED_MULTIPLIER_WHEN_IDLING = 0.6F;

    public static List<ActivityData<Piglin>> getActivities(Piglin piglin) {
        return List.of(coreActivity(), idleActivity(), admireItemActivity(), fightActivity(piglin), celebrateActivity(), retreatActivity(), rideHoglinActivity());
    }

    protected static void initMemories(Piglin body, RandomSource random) {
        body.getBrain().setMemoryWithExpiry(MemoryModuleType.HUNTED_RECENTLY, true, TIME_BETWEEN_HUNTS.sample(random));
    }

    private static ActivityData<Piglin> coreActivity() {
        return ActivityData.<Piglin>create(Activity.CORE, 0, ImmutableList.<BehaviorControl<? super Piglin>>of(new LookAtTargetSink(45, 90), new MoveToTargetSink(),
            InteractWithDoor.create(), babyAvoidNemesis(), avoidZombified(), StopHoldingItemIfNoLongerAdmiring.create(), StartAdmiringItemIfSeen.create(ADMIRE_DURATION),
            StartCelebratingIfTargetDead.create(CELEBRATION_TIME, PiglinAi::wantsToDance), StopBeingAngryIfTargetDead.create()));
    }

    private static ActivityData<Piglin> idleActivity() {
        return ActivityData.<Piglin>create(Activity.IDLE, 10, ImmutableList.<BehaviorControl<? super Piglin>>of(
            SetEntityLookTarget.create(PiglinAi::isPlayerHoldingLovedItem, MAX_LOOK_DIST_FOR_PLAYER_HOLDING_LOVED_ITEM),
            StartAttacking.<Piglin>create((level, piglin) -> piglin.isAdult(), PiglinAi::findNearestValidAttackTarget),
            BehaviorBuilder.<Piglin>triggerIf(Piglin::canHunt, StartHuntingHoglin.create()), avoidRepellent(), babySometimesRideBabyHoglin(),
            idleLookBehaviors(), idleMovementBehaviors(), SetLookAndInteract.create(EntityTypes.PLAYER, 4)));
    }

    private static ActivityData<Piglin> fightActivity(Piglin body) {
        return ActivityData.<Piglin>create(Activity.FIGHT, 10, ImmutableList.<BehaviorControl<? super Piglin>>of(
            StopAttackingIfTargetInvalid.<Piglin>create((level, target) -> !isNearestValidAttackTarget(level, body, target)),
            BehaviorBuilder.<Piglin>triggerIf(PiglinAi::hasCrossbow, BackUpIfTooClose.create(MIN_DESIRED_DIST_FROM_TARGET_WHEN_HOLDING_CROSSBOW, SPEED_WHEN_STRAFING_BACK_FROM_TARGET)),
            SetWalkTargetFromAttackTargetIfTargetOutOfReach.create(1.0F), new SpearApproach(1.0, 10.0F), new SpearAttack(1.0, 1.0, 2.0F), new SpearRetreat(1.0),
            MeleeAttack.<Piglin>create(MELEE_ATTACK_COOLDOWN), new CrossbowAttack<Piglin, LivingEntity>(), RememberIfHoglinWasKilled.create(),
            EraseMemoryIf.<Piglin>create(PiglinAi::isNearZombified, MemoryModuleType.ATTACK_TARGET)), MemoryModuleType.ATTACK_TARGET);
    }

    private static ActivityData<Piglin> celebrateActivity() {
        return ActivityData.<Piglin>create(Activity.CELEBRATE, 10, ImmutableList.<BehaviorControl<? super Piglin>>of(avoidRepellent(),
            SetEntityLookTarget.create(PiglinAi::isPlayerHoldingLovedItem, MAX_LOOK_DIST_FOR_PLAYER_HOLDING_LOVED_ITEM),
            StartAttacking.<Piglin>create((level, piglin) -> piglin.isAdult(), PiglinAi::findNearestValidAttackTarget),
            BehaviorBuilder.<Piglin>triggerIf(piglin -> !piglin.isDancing(), GoToTargetLocation.<Piglin>create(MemoryModuleType.CELEBRATE_LOCATION, 2, SPEED_MULTIPLIER_WHEN_GOING_TO_CELEBRATE_LOCATION)),
            BehaviorBuilder.<Piglin>triggerIf(Piglin::isDancing, GoToTargetLocation.<Piglin>create(MemoryModuleType.CELEBRATE_LOCATION, 4, SPEED_MULTIPLIER_WHEN_DANCING)),
            new RunOne<Piglin>(ImmutableList.of(Pair.of(SetEntityLookTarget.create(EntityTypes.PIGLIN, 8.0F), 1), Pair.of(RandomStroll.stroll(SPEED_MULTIPLIER_WHEN_DANCING, 2, 1), 1),
                Pair.of(new DoNothing(10, 20), 1)))), MemoryModuleType.CELEBRATE_LOCATION);
    }

    private static ActivityData<Piglin> admireItemActivity() {
        return ActivityData.<Piglin>create(Activity.ADMIRE_ITEM, 10, ImmutableList.<BehaviorControl<? super Piglin>>of(
            GoToWantedItem.<Piglin>create(PiglinAi::isNotHoldingLovedItemInOffHand, SPEED_MULTIPLIER_WHEN_GOING_TO_WANTED_ITEM, true, MAX_DISTANCE_TO_WALK_TO_ITEM),
            StopAdmiringIfItemTooFarAway.create(MAX_DISTANCE_TO_WALK_TO_ITEM),
            StopAdmiringIfTiredOfTryingToReachItem.create(MAX_TIME_TO_WALK_TO_ITEM, HOW_LONG_TIME_TO_DISABLE_ADMIRE_WALKING_IF_CANT_REACH_ITEM)), MemoryModuleType.ADMIRING_ITEM);
    }

    private static ActivityData<Piglin> retreatActivity() {
        return ActivityData.<Piglin>create(Activity.AVOID, 10, ImmutableList.<BehaviorControl<? super Piglin>>of(
            SetWalkTargetAwayFrom.entity(MemoryModuleType.AVOID_TARGET, SPEED_MULTIPLIER_WHEN_RETREATING, DESIRED_DISTANCE_FROM_ENTITY_WHEN_AVOIDING, true),
            idleLookBehaviors(), idleMovementBehaviors(), EraseMemoryIf.<Piglin>create(PiglinAi::wantsToStopFleeing, MemoryModuleType.AVOID_TARGET)), MemoryModuleType.AVOID_TARGET);
    }

    private static ActivityData<Piglin> rideHoglinActivity() {
        OneShot<LivingEntity> lookWhileRiding = BehaviorBuilder.<LivingEntity>triggerIf(Entity::isPassenger, TriggerGate.<LivingEntity>triggerOneShuffled(rideLookTriggers()));
        return ActivityData.<Piglin>create(Activity.RIDE, 10, ImmutableList.<BehaviorControl<? super Piglin>>of(Mount.create(SPEED_MULTIPLIER_WHEN_MOUNTING),
            SetEntityLookTarget.create(PiglinAi::isPlayerHoldingLovedItem, MAX_LOOK_DIST), lookWhileRiding,
            DismountOrSkipMounting.<Piglin>create(MAX_WALK_DISTANCE_TO_START_RIDING, PiglinAi::wantsToStopRiding)), MemoryModuleType.RIDE_TARGET);
    }

    private static List<Pair<? extends Trigger<? super LivingEntity>, Integer>> rideLookTriggers() {
        List<Pair<? extends Trigger<? super LivingEntity>, Integer>> triggers = new ArrayList<>();
        for (Pair<OneShot<LivingEntity>, Integer> look : lookBehaviors()) {
            triggers.add(look);
        }
        triggers.add(Pair.of(BehaviorBuilder.<LivingEntity>triggerIf(e -> true), 1));
        return triggers;
    }

    private static ImmutableList<Pair<OneShot<LivingEntity>, Integer>> lookBehaviors() {
        return ImmutableList.<Pair<OneShot<LivingEntity>, Integer>>of(Pair.of(SetEntityLookTarget.create(EntityTypes.PLAYER, 8.0F), 1),
            Pair.of(SetEntityLookTarget.create(EntityTypes.PIGLIN, 8.0F), 1), Pair.of(SetEntityLookTarget.create(8.0F), 1));
    }

    private static RunOne<LivingEntity> idleLookBehaviors() {
        List<Pair<? extends BehaviorControl<? super LivingEntity>, Integer>> list = new ArrayList<>();
        list.addAll(lookBehaviors());
        list.add(Pair.of(new DoNothing(30, 60), 1));
        return new RunOne<LivingEntity>(list);
    }

    private static RunOne<Piglin> idleMovementBehaviors() {
        return new RunOne<Piglin>(ImmutableList.of(Pair.of(RandomStroll.stroll(SPEED_MULTIPLIER_WHEN_IDLING), 2),
            Pair.of(InteractWith.of(EntityTypes.PIGLIN, INTERACTION_RANGE, MemoryModuleType.INTERACTION_TARGET, SPEED_MULTIPLIER_WHEN_IDLING, 2), 2),
            Pair.of(BehaviorBuilder.<Piglin>triggerIf(PiglinAi::doesntSeeAnyPlayerHoldingLovedItem, SetWalkTargetFromLookTarget.create(SPEED_MULTIPLIER_WHEN_IDLING, 3)), 2),
            Pair.of(new DoNothing(30, 60), 1)));
    }

    private static BehaviorControl<PathfinderMob> avoidRepellent() {
        return SetWalkTargetAwayFrom.pos(MemoryModuleType.NEAREST_REPELLENT, SPEED_MULTIPLIER_WHEN_AVOIDING, REPELLENT_DETECTION_RANGE_HORIZONTAL, false);
    }

    private static BehaviorControl<Piglin> babyAvoidNemesis() {
        return CopyMemoryWithExpiry.create(Piglin::isBaby, MemoryModuleType.NEAREST_VISIBLE_NEMESIS, MemoryModuleType.AVOID_TARGET, BABY_AVOID_NEMESIS_DURATION);
    }

    private static BehaviorControl<Piglin> avoidZombified() {
        return CopyMemoryWithExpiry.create(PiglinAi::isNearZombified, MemoryModuleType.NEAREST_VISIBLE_ZOMBIFIED, MemoryModuleType.AVOID_TARGET, AVOID_ZOMBIFIED_DURATION);
    }

    protected static void updateActivity(Piglin body) {
        Brain<Piglin> mind = body.getBrain();
        Activity before = mind.getActiveNonCoreActivity().orElse(null);
        mind.setActiveActivityToFirstValid(List.of(Activity.ADMIRE_ITEM, Activity.FIGHT, Activity.AVOID, Activity.CELEBRATE,
            Activity.RIDE, Activity.IDLE));
        Activity after = mind.getActiveNonCoreActivity().orElse(null);
        if (before != after) {
            getSoundForCurrentActivity(body).ifPresent(body::makeSound);
        }
        body.setAggressive(mind.hasMemoryValue(MemoryModuleType.ATTACK_TARGET));
        boolean mountless = !mind.hasMemoryValue(MemoryModuleType.RIDE_TARGET);
        if (mountless && isBabyRidingBaby(body)) {
            body.stopRiding();
        }
        boolean partyOver = !mind.hasMemoryValue(MemoryModuleType.CELEBRATE_LOCATION);
        if (partyOver) {
            mind.eraseMemory(MemoryModuleType.DANCING);
        }
        body.setDancing(mind.hasMemoryValue(MemoryModuleType.DANCING));
    }

    private static boolean isBabyRidingBaby(Piglin body) {
        if (!body.isBaby()) {
            return false;
        }
        Entity vehicle = body.getVehicle();
        return vehicle instanceof Piglin piglin && piglin.isBaby() || vehicle instanceof Hoglin hoglin && hoglin.isBaby();
    }

    protected static void pickUpItem(ServerLevel level, Piglin body, ItemEntity itemEntity) {
        stopWalking(body);
        ItemStack stack = itemEntity.getItem();
        ItemStack grabbed;
        if (stack.is(Items.GOLD_NUGGET)) {
            int amount = stack.getCount();
            body.take(itemEntity, amount);
            itemEntity.discard();
            grabbed = stack;
        } else {
            body.take(itemEntity, 1);
            grabbed = removeOneItemFromItemEntity(itemEntity);
        }
        if (isLovedItem(grabbed)) {
            body.getBrain().eraseMemory(MemoryModuleType.TIME_TRYING_TO_REACH_ADMIRE_ITEM);
            holdInOffhand(level, body, grabbed);
            admireGoldItem(body);
            return;
        }
        if (isFood(grabbed) && !hasEatenRecently(body)) {
            eat(body);
            return;
        }
        ItemStack leftover = body.equipItemIfPossible(level, grabbed);
        if (leftover.isEmpty()) {
            putInInventory(body, grabbed);
        }
    }

    private static void holdInOffhand(ServerLevel level, Piglin body, ItemStack item) {
        if (isHoldingItemInOffHand(body)) {
            ItemStack old = body.getItemInHand(InteractionHand.OFF_HAND);
            body.spawnAtLocation(level, old);
        }
        body.holdInOffHand(item);
    }

    private static ItemStack removeOneItemFromItemEntity(ItemEntity itemEntity) {
        ItemStack source = itemEntity.getItem();
        ItemStack removed = source.split(1);
        if (source.isEmpty()) {
            itemEntity.discard();
        } else {
            itemEntity.setItem(source);
        }
        return removed;
    }

    protected static void stopHoldingOffHandItem(ServerLevel level, Piglin body, boolean barteringEnabled) {
        ItemStack held = body.getItemInHand(InteractionHand.OFF_HAND);
        body.setItemInHand(InteractionHand.OFF_HAND, ItemStack.EMPTY);
        if (!body.isAdult()) {
            if (body.equipItemIfPossible(level, held).isEmpty()) {
                ItemStack inHand = body.getMainHandItem();
                if (isLovedItem(inHand)) {
                    putInInventory(body, inHand);
                } else {
                    throwItems(body, Collections.singletonList(inHand));
                }
                body.holdInMainHand(held);
            }
            return;
        }
        if (isBarterCurrency(held)) {
            if (barteringEnabled) {
                barter(body);
            }
            return;
        }
        if (body.equipItemIfPossible(level, held).isEmpty()) {
            putInInventory(body, held);
        }
    }

    protected static void cancelAdmiring(ServerLevel level, Piglin body) {
        if (!isAdmiringItem(body)) {
            return;
        }
        ItemStack off = body.getOffhandItem();
        if (!off.isEmpty()) {
            body.spawnAtLocation(level, off);
            body.setItemInHand(InteractionHand.OFF_HAND, ItemStack.EMPTY);
        }
    }

    private static void putInInventory(Piglin body, ItemStack itemStack) {
        throwItemsTowardRandomPos(body, Collections.singletonList(body.addToInventory(itemStack)));
    }

    private static void throwItems(Piglin body, List<ItemStack> itemStacks) {
        Optional<Player> player = body.getBrain().getMemory(MemoryModuleType.NEAREST_VISIBLE_PLAYER);
        if (player.isPresent()) {
            throwItemsTowardPos(body, itemStacks, player.get().position());
        } else {
            throwItemsTowardRandomPos(body, itemStacks);
        }
    }

    private static void throwItemsTowardRandomPos(Piglin body, List<ItemStack> itemStacks) {
        throwItemsTowardPos(body, itemStacks, getRandomNearbyPos(body));
    }

    private static void throwItemsTowardPos(Piglin body, List<ItemStack> itemStacks, Vec3 targetPos) {
        if (!itemStacks.isEmpty()) {
            body.swing(InteractionHand.OFF_HAND, net.minecraft.world.item.component.SwingAnimation.DEFAULT, true);
            for (ItemStack itemStack : itemStacks) {
                BehaviorUtils.throwItem(body, itemStack, targetPos.add(0.0, 1.0, 0.0));
            }
        }
    }

    private static void barter(Piglin body) {
        if (!(body.live() instanceof org.bukkit.entity.Piglin api)) {
            return;
        }
        org.bukkit.loot.LootTable table = org.bukkit.Bukkit.getLootTable(org.bukkit.loot.LootTables.PIGLIN_BARTERING.getKey());
        if (table == null) {
            return;
        }
        Vec3 target = body.getBrain().getMemory(MemoryModuleType.NEAREST_VISIBLE_PLAYER).map(Entity::position).orElseGet(() -> getRandomNearbyPos(body));
        org.bukkit.loot.LootContext context = new org.bukkit.loot.LootContext.Builder(api.getLocation()).lootedEntity(api).build();
        org.bukkit.util.Vector direction = new org.bukkit.util.Vector(target.x - body.getX(), target.y + 1.0 - body.getEyeY(), target.z - body.getZ()).normalize().multiply(0.3);
        for (org.bukkit.inventory.ItemStack stack : table.populateLoot(new java.util.Random(body.level().getRandom().nextLong()), context)) {
            api.getWorld().dropItem(api.getEyeLocation().subtract(0.0, 0.3, 0.0), stack, item -> item.setVelocity(direction));
        }
        body.swing(InteractionHand.OFF_HAND, net.minecraft.world.item.component.SwingAnimation.DEFAULT, true);
    }

    private static boolean wantsToDance(LivingEntity body, LivingEntity killedTarget) {
        return killedTarget.is(EntityTypes.HOGLIN) && RandomSource.createThreadLocalInstance(body.level().getGameTime()).nextFloat() < PROBABILITY_OF_CELEBRATION_DANCE;
    }

    protected static boolean wantsToPickup(Piglin body, ItemStack stack) {
        boolean rejected = body.isBaby() && stack.is(ItemTags.IGNORED_BY_PIGLIN_BABIES)
            || stack.is(ItemTags.PIGLIN_REPELLENTS)
            || isAdmiringDisabled(body) && body.getBrain().hasMemoryValue(MemoryModuleType.ATTACK_TARGET);
        if (rejected) {
            return false;
        }
        if (isBarterCurrency(stack)) {
            return isNotHoldingLovedItemInOffHand(body);
        }
        boolean roomLeft = body.canAddToInventory(stack);
        if (stack.is(Items.GOLD_NUGGET)) {
            return roomLeft;
        }
        if (isFood(stack)) {
            return roomLeft && !hasEatenRecently(body);
        }
        return isLovedItem(stack) ? roomLeft && isNotHoldingLovedItemInOffHand(body) : body.canReplaceCurrentItem(stack);
    }

    protected static boolean isLovedItem(ItemStack itemStack) {
        return itemStack.is(ItemTags.PIGLIN_LOVED);
    }

    private static boolean wantsToStopRiding(Piglin body, Entity mount) {
        if (mount instanceof Mob rideable) {
            if (rideable.isBaby() && rideable.isAlive() && !wasHurtRecently(body) && !wasHurtRecently(rideable)) {
                return rideable instanceof Piglin && rideable.getVehicle() == null;
            }
            return true;
        }
        return false;
    }

    private static boolean isNearestValidAttackTarget(ServerLevel level, Piglin body, LivingEntity target) {
        return findNearestValidAttackTarget(level, body).filter(nearest -> nearest == target).isPresent();
    }

    private static boolean isNearZombified(Piglin body) {
        Brain<Piglin> brain = body.getBrain();
        Optional<LivingEntity> zombified = brain.getMemory(MemoryModuleType.NEAREST_VISIBLE_ZOMBIFIED);
        return zombified.isPresent() && body.closerThan(zombified.get(), DESIRED_DISTANCE_FROM_ZOMBIFIED);
    }

    private static Optional<? extends LivingEntity> findNearestValidAttackTarget(ServerLevel level, Piglin body) {
        if (isNearZombified(body)) {
            return Optional.empty();
        }
        Brain<Piglin> brain = body.getBrain();
        Optional<LivingEntity> grudge = BehaviorUtils.getLivingEntityFromUUIDMemory(body, MemoryModuleType.ANGRY_AT)
            .filter(target -> Sensor.isEntityAttackableIgnoringLineOfSight(level, body, target));
        if (grudge.isPresent()) {
            return grudge;
        }
        if (brain.hasMemoryValue(MemoryModuleType.UNIVERSAL_ANGER)) {
            Optional<Player> anyPlayer = brain.getMemory(MemoryModuleType.NEAREST_VISIBLE_ATTACKABLE_PLAYER);
            if (anyPlayer.isPresent()) {
                return anyPlayer;
            }
        }
        Optional<Mob> enemy = brain.getMemory(MemoryModuleType.NEAREST_VISIBLE_NEMESIS);
        if (enemy.isPresent()) {
            return enemy;
        }
        return brain.getMemory(MemoryModuleType.NEAREST_TARGETABLE_PLAYER_NOT_WEARING_GOLD)
            .filter(player -> Sensor.isEntityAttackable(level, body, player));
    }

    public static void angerNearbyPiglins(ServerLevel level, Player player, boolean onlyIfTheySeeThePlayer) {
        boolean universal = level.getGameRules().get(GameRules.UNIVERSAL_ANGER);
        for (Piglin piglin : player.level().getEntitiesOfClass(Piglin.class, player.getBoundingBox().inflate(PLAYER_ANGER_RANGE))) {
            if (!isIdle(piglin) || onlyIfTheySeeThePlayer && !BehaviorUtils.canSee(piglin, player)) {
                continue;
            }
            if (universal) {
                setAngerTargetToNearestTargetablePlayerIfFound(level, piglin, player);
            } else {
                setAngerTarget(level, piglin, player);
            }
        }
    }

    public static InteractionResult mobInteract(ServerLevel level, Piglin body, Player player, InteractionHand hand) {
        ItemStack held = player.getItemInHand(hand);
        if (canAdmire(body, held)) {
            ItemStack taken = held.split(1);
            holdInOffhand(level, body, taken);
            admireGoldItem(body);
            stopWalking(body);
            return InteractionResult.SUCCESS;
        }
        return InteractionResult.PASS;
    }

    protected static boolean canAdmire(Piglin body, ItemStack playerHeldItemStack) {
        return !isAdmiringDisabled(body) && !isAdmiringItem(body) && body.isAdult() && isBarterCurrency(playerHeldItemStack);
    }

    protected static void wasHurtBy(ServerLevel level, Piglin body, LivingEntity attacker) {
        if (attacker instanceof Piglin) {
            return;
        }
        if (isHoldingItemInOffHand(body)) {
            stopHoldingOffHandItem(level, body, false);
        }
        Brain<Piglin> mind = body.getBrain();
        for (MemoryModuleType<?> forgotten : List.of(MemoryModuleType.CELEBRATE_LOCATION, MemoryModuleType.DANCING,
            MemoryModuleType.ADMIRING_ITEM)) {
            mind.eraseMemory(forgotten);
        }
        if (attacker instanceof Player) {
            mind.setMemoryWithExpiry(MemoryModuleType.ADMIRING_DISABLED, true, HIT_BY_PLAYER_MEMORY_TIMEOUT);
        }
        Optional<LivingEntity> fleeing = getAvoidTarget(body);
        if (fleeing.isPresent() && fleeing.get().getType() != attacker.getType()) {
            mind.eraseMemory(MemoryModuleType.AVOID_TARGET);
        }
        if (body.isBaby()) {
            mind.setMemoryWithExpiry(MemoryModuleType.AVOID_TARGET, attacker, BABY_FLEE_DURATION_AFTER_GETTING_HIT);
            if (Sensor.isEntityAttackableIgnoringLineOfSight(level, body, attacker)) {
                broadcastAngerTarget(level, body, attacker);
            }
            return;
        }
        if (attacker.is(EntityTypes.HOGLIN) && hoglinsOutnumberPiglins(body)) {
            setAvoidTargetAndDontHuntForAWhile(body, attacker);
            broadcastRetreat(body, attacker);
            return;
        }
        maybeRetaliate(level, body, attacker);
    }

    protected static void maybeRetaliate(ServerLevel level, AbstractPiglin body, LivingEntity attacker) {
        boolean ignore = body.getBrain().isActive(Activity.AVOID)
            || !Sensor.isEntityAttackableIgnoringLineOfSight(level, body, attacker)
            || BehaviorUtils.isOtherTargetMuchFurtherAwayThanCurrentAttackTarget(body, attacker, 4.0);
        if (ignore) {
            return;
        }
        boolean mobbing = attacker.is(EntityTypes.PLAYER) && level.getGameRules().get(GameRules.UNIVERSAL_ANGER);
        if (mobbing) {
            setAngerTargetToNearestTargetablePlayerIfFound(level, body, attacker);
            broadcastUniversalAnger(level, body);
            return;
        }
        setAngerTarget(level, body, attacker);
        broadcastAngerTarget(level, body, attacker);
    }

    public static Optional<SoundEvent> getSoundForCurrentActivity(Piglin body) {
        return body.getBrain().getActiveNonCoreActivity().map(activity -> getSoundForActivity(body, activity));
    }

    private static SoundEvent getSoundForActivity(Piglin body, Activity activity) {
        if (activity == Activity.FIGHT) {
            return SoundEvents.PIGLIN_ANGRY;
        }
        boolean fleeing = body.isConverting() || activity == Activity.AVOID && isNearAvoidTarget(body);
        if (fleeing) {
            return SoundEvents.PIGLIN_RETREAT;
        }
        if (activity == Activity.ADMIRE_ITEM) {
            return SoundEvents.PIGLIN_ADMIRING_ITEM;
        }
        if (activity == Activity.CELEBRATE) {
            return SoundEvents.PIGLIN_CELEBRATE;
        }
        if (seesPlayerHoldingLovedItem(body)) {
            return SoundEvents.PIGLIN_JEALOUS;
        }
        return isNearRepellent(body) ? SoundEvents.PIGLIN_RETREAT : SoundEvents.PIGLIN_AMBIENT;
    }

    private static boolean isNearAvoidTarget(Piglin body) {
        return getAvoidTarget(body).filter(scary -> scary.closerThan(body, DESIRED_DISTANCE_FROM_ENTITY_WHEN_AVOIDING)).isPresent();
    }

    protected static List<AbstractPiglin> getVisibleAdultPiglins(Piglin body) {
        return body.getBrain().getMemory(MemoryModuleType.NEAREST_VISIBLE_ADULT_PIGLINS).orElse(ImmutableList.of());
    }

    private static List<AbstractPiglin> getAdultPiglins(AbstractPiglin body) {
        return body.getBrain().getMemory(MemoryModuleType.NEARBY_ADULT_PIGLINS).orElse(ImmutableList.of());
    }

    public static boolean isWearingSafeArmor(LivingEntity wearer) {
        for (net.minecraft.world.entity.EquipmentSlot slot : net.minecraft.world.entity.EquipmentSlotGroup.ARMOR) {
            if (wearer.getItemBySlot(slot).is(ItemTags.PIGLIN_SAFE_ARMOR)) {
                return true;
            }
        }
        return false;
    }

    private static void stopWalking(Piglin body) {
        body.getNavigation().stop();
        body.getBrain().eraseMemory(MemoryModuleType.WALK_TARGET);
    }

    private static BehaviorControl<LivingEntity> babySometimesRideBabyHoglin() {
        SetEntityLookTargetSometimes.Ticker ticker = new SetEntityLookTargetSometimes.Ticker(RIDE_START_INTERVAL);
        return CopyMemoryWithExpiry.create(e -> e.isBaby() && ticker.tickDownAndCheck(e.level().getRandom()), MemoryModuleType.NEAREST_VISIBLE_BABY_HOGLIN,
            MemoryModuleType.RIDE_TARGET, RIDE_DURATION);
    }

    protected static void broadcastAngerTarget(ServerLevel level, AbstractPiglin body, LivingEntity target) {
        for (AbstractPiglin peer : getAdultPiglins(body)) {
            boolean unhuntable = target instanceof Hoglin prey && (!peer.canHunt() || !prey.canBeHunted());
            if (!unhuntable) {
                setAngerTargetIfCloserThanCurrent(level, peer, target);
            }
        }
    }

    protected static void broadcastUniversalAnger(ServerLevel level, AbstractPiglin body) {
        for (AbstractPiglin peer : getAdultPiglins(body)) {
            Optional<Player> victim = getNearestVisibleTargetablePlayer(peer);
            victim.ifPresent(player -> setAngerTarget(level, peer, player));
        }
    }

    protected static void setAngerTarget(ServerLevel level, AbstractPiglin body, LivingEntity target) {
        if (!Sensor.isEntityAttackableIgnoringLineOfSight(level, body, target)) {
            return;
        }
        Brain<?> mind = body.getBrain();
        mind.eraseMemory(MemoryModuleType.CANT_REACH_WALK_TARGET_SINCE);
        mind.setMemoryWithExpiry(MemoryModuleType.ANGRY_AT, target.getUUID(), ANGER_DURATION);
        if (body.canHunt() && target.is(EntityTypes.HOGLIN)) {
            dontKillAnyMoreHoglinsForAWhile(body);
        }
        if (target.is(EntityTypes.PLAYER) && level.getGameRules().get(GameRules.UNIVERSAL_ANGER)) {
            mind.setMemoryWithExpiry(MemoryModuleType.UNIVERSAL_ANGER, true, ANGER_DURATION);
        }
    }

    private static void setAngerTargetToNearestTargetablePlayerIfFound(ServerLevel level, AbstractPiglin body, LivingEntity targetIfNoPlayerFound) {
        Optional<Player> nearest = getNearestVisibleTargetablePlayer(body);
        setAngerTarget(level, body, nearest.isPresent() ? nearest.get() : targetIfNoPlayerFound);
    }

    private static void setAngerTargetIfCloserThanCurrent(ServerLevel level, AbstractPiglin body, LivingEntity newTarget) {
        Optional<LivingEntity> current = getAngerTarget(body);
        LivingEntity nearest = BehaviorUtils.getNearestTarget(body, current, newTarget);
        if (current.isEmpty() || current.get() != nearest) {
            setAngerTarget(level, body, nearest);
        }
    }

    private static Optional<LivingEntity> getAngerTarget(AbstractPiglin body) {
        return BehaviorUtils.getLivingEntityFromUUIDMemory(body, MemoryModuleType.ANGRY_AT);
    }

    public static Optional<LivingEntity> getAvoidTarget(Piglin body) {
        return body.getBrain().hasMemoryValue(MemoryModuleType.AVOID_TARGET) ? body.getBrain().getMemory(MemoryModuleType.AVOID_TARGET) : Optional.empty();
    }

    public static Optional<Player> getNearestVisibleTargetablePlayer(AbstractPiglin body) {
        return body.getBrain().hasMemoryValue(MemoryModuleType.NEAREST_VISIBLE_ATTACKABLE_PLAYER) ? body.getBrain().getMemory(MemoryModuleType.NEAREST_VISIBLE_ATTACKABLE_PLAYER) : Optional.empty();
    }

    private static void broadcastRetreat(Piglin body, LivingEntity target) {
        for (AbstractPiglin peer : getVisibleAdultPiglins(body)) {
            if (peer instanceof Piglin fellow) {
                retreatFromNearestTarget(fellow, target);
            }
        }
    }

    private static void retreatFromNearestTarget(Piglin body, LivingEntity newAvoidTarget) {
        Brain<Piglin> mind = body.getBrain();
        LivingEntity closestFear = BehaviorUtils.getNearestTarget(body, mind.getMemory(MemoryModuleType.AVOID_TARGET), newAvoidTarget);
        LivingEntity closest = BehaviorUtils.getNearestTarget(body, mind.getMemory(MemoryModuleType.ATTACK_TARGET), closestFear);
        setAvoidTargetAndDontHuntForAWhile(body, closest);
    }

    private static boolean wantsToStopFleeing(Piglin body) {
        Brain<Piglin> mind = body.getBrain();
        Optional<LivingEntity> fear = mind.getMemory(MemoryModuleType.AVOID_TARGET);
        if (fear.isEmpty()) {
            return true;
        }
        LivingEntity scary = fear.get();
        if (scary.is(EntityTypes.HOGLIN)) {
            return !hoglinsOutnumberPiglins(body);
        }
        return isZombified(scary) && !mind.isMemoryValue(MemoryModuleType.NEAREST_VISIBLE_ZOMBIFIED, scary);
    }

    private static boolean hoglinsOutnumberPiglins(Piglin body) {
        int piglins = body.getBrain().getMemory(MemoryModuleType.VISIBLE_ADULT_PIGLIN_COUNT).orElse(0) + 1;
        int hoglins = body.getBrain().getMemory(MemoryModuleType.VISIBLE_ADULT_HOGLIN_COUNT).orElse(0);
        return hoglins > piglins;
    }

    private static void setAvoidTargetAndDontHuntForAWhile(Piglin body, LivingEntity target) {
        Brain<Piglin> mind = body.getBrain();
        mind.eraseMemory(MemoryModuleType.ANGRY_AT);
        mind.eraseMemory(MemoryModuleType.ATTACK_TARGET);
        mind.eraseMemory(MemoryModuleType.WALK_TARGET);
        int ticks = RETREAT_DURATION.sample(body.level().getRandom());
        mind.setMemoryWithExpiry(MemoryModuleType.AVOID_TARGET, target, ticks);
        dontKillAnyMoreHoglinsForAWhile(body);
    }

    protected static void dontKillAnyMoreHoglinsForAWhile(AbstractPiglin body) {
        body.getBrain().setMemoryWithExpiry(MemoryModuleType.HUNTED_RECENTLY, true, TIME_BETWEEN_HUNTS.sample(body.level().getRandom()));
    }

    private static void eat(Piglin body) {
        body.getBrain().setMemoryWithExpiry(MemoryModuleType.ATE_RECENTLY, true, EAT_COOLDOWN);
    }

    private static Vec3 getRandomNearbyPos(Piglin body) {
        Vec3 target = LandRandomPos.getPos(body, 4, 2);
        return target == null ? body.position() : target;
    }

    private static boolean hasEatenRecently(Piglin body) {
        return body.getBrain().hasMemoryValue(MemoryModuleType.ATE_RECENTLY);
    }

    protected static boolean isIdle(AbstractPiglin body) {
        return body.getBrain().isActive(Activity.IDLE);
    }

    private static boolean hasCrossbow(LivingEntity body) {
        return body.isHolding(Items.CROSSBOW);
    }

    private static void admireGoldItem(LivingEntity body) {
        body.getBrain().setMemoryWithExpiry(MemoryModuleType.ADMIRING_ITEM, true, ADMIRE_DURATION);
    }

    private static boolean isAdmiringItem(Piglin body) {
        return body.getBrain().hasMemoryValue(MemoryModuleType.ADMIRING_ITEM);
    }

    private static boolean isBarterCurrency(ItemStack itemStack) {
        return itemStack.is(BARTERING_ITEM);
    }

    private static boolean isFood(ItemStack itemStack) {
        return itemStack.is(ItemTags.PIGLIN_FOOD);
    }

    private static boolean isNearRepellent(Piglin body) {
        return body.getBrain().hasMemoryValue(MemoryModuleType.NEAREST_REPELLENT);
    }

    private static boolean seesPlayerHoldingLovedItem(LivingEntity body) {
        return body.getBrain().hasMemoryValue(MemoryModuleType.NEAREST_PLAYER_HOLDING_WANTED_ITEM);
    }

    private static boolean doesntSeeAnyPlayerHoldingLovedItem(LivingEntity body) {
        return !seesPlayerHoldingLovedItem(body);
    }

    public static boolean isPlayerHoldingLovedItem(LivingEntity entity) {
        return entity.is(EntityTypes.PLAYER) && entity.isHolding(PiglinAi::isLovedItem);
    }

    private static boolean isAdmiringDisabled(Piglin body) {
        return body.getBrain().hasMemoryValue(MemoryModuleType.ADMIRING_DISABLED);
    }

    private static boolean wasHurtRecently(LivingEntity body) {
        return body.getBrain().hasMemoryValue(MemoryModuleType.HURT_BY);
    }

    private static boolean isHoldingItemInOffHand(Piglin body) {
        return !body.getOffhandItem().isEmpty();
    }

    private static boolean isNotHoldingLovedItemInOffHand(Piglin body) {
        return body.getOffhandItem().isEmpty() || !isLovedItem(body.getOffhandItem());
    }

    public static boolean isZombified(Entity entity) {
        return entity.is(EntityTypes.ZOMBIFIED_PIGLIN) || entity.is(EntityTypes.ZOGLIN);
    }

    public static List<AbstractPiglin> findNearbyAdultPiglins(Brain<?> brain) {
        List<LivingEntity> livingEntities = brain.getMemory(MemoryModuleType.NEAREST_LIVING_ENTITIES).orElse(List.of());
        List<AbstractPiglin> adults = new ArrayList<>();
        for (LivingEntity entity : livingEntities) {
            if (entity instanceof AbstractPiglin piglin && piglin.isAdult()) {
                adults.add(piglin);
            }
        }
        return adults;
    }
}
