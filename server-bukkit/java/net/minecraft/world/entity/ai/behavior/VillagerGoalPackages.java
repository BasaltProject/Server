// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.ai.behavior;

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
import com.google.common.collect.ImmutableList;
import com.google.common.collect.ImmutableSet;
import java.util.ArrayList;
import java.util.List;
import java.util.Map;
import java.util.Optional;
import net.minecraft.core.BlockPos;
import net.minecraft.core.Holder;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.world.entity.AgeableMob;
import net.minecraft.world.entity.EntityTypes;
import net.minecraft.world.entity.LivingEntity;
import net.minecraft.world.entity.MobCategory;
import net.minecraft.world.entity.ai.behavior.declarative.BehaviorBuilder;
import net.minecraft.world.entity.ai.behavior.declarative.Trigger;
import net.minecraft.world.entity.ai.memory.MemoryModuleType;
import net.minecraft.world.entity.ai.memory.MemoryStatus;
import net.minecraft.world.entity.ai.village.poi.PoiTypes;
import net.minecraft.world.entity.npc.villager.Villager;
import net.minecraft.world.entity.npc.villager.VillagerProfession;
import net.minecraft.world.level.block.BedBlock;

@SuppressWarnings({"unchecked", "rawtypes"})
public class VillagerGoalPackages {
    private static final float STROLL_SPEED_MODIFIER = 0.4F;
    public static final int INTERACT_DIST_SQR = 5;
    public static final int INTERACT_WALKUP_DIST = 2;
    public static final float INTERACT_SPEED_MODIFIER = 0.5F;

    private static ImmutableList<Pair<Integer, ? extends BehaviorControl<? super Villager>>> pack(Pair... entries) {
        return ImmutableList.copyOf((Pair<Integer, ? extends BehaviorControl<? super Villager>>[]) entries);
    }

    private static Pair<Integer, BehaviorControl> p(int priority, BehaviorControl behavior) {
        return Pair.of(priority, behavior);
    }

    private static Pair<BehaviorControl, Integer> wt(BehaviorControl behavior, int weight) {
        return Pair.of(behavior, weight);
    }

    private static List weighted(Pair... entries) {
        return List.of(entries);
    }

    public static ImmutableList<Pair<Integer, ? extends BehaviorControl<? super Villager>>> getCorePackage(Holder<VillagerProfession> profession, float speedModifier) {
        VillagerProfession value = profession.value();
        return pack(p(0, new Swim(0.8F)), p(0, InteractWithDoor.create()), p(0, new LookAtTargetSink(45, 90)), p(0, new VillagerPanicTrigger()), p(0, WakeUp.create()),
            p(0, ReactToBell.create()), p(0, SetRaidStatus.create()), p(0, ValidateNearbyPoi.create(value.heldJobSite(), MemoryModuleType.JOB_SITE)),
            p(0, ValidateNearbyPoi.create(value.acquirableJobSite(), MemoryModuleType.POTENTIAL_JOB_SITE)), p(1, new MoveToTargetSink()), p(2, PoiCompetitorScan.create()),
            p(3, new LookAndFollowTradingPlayerSink(speedModifier)), p(5, GoToWantedItem.create(speedModifier, false, 4)),
            p(6, AcquirePoi.create(value.acquirableJobSite(), MemoryModuleType.JOB_SITE, MemoryModuleType.POTENTIAL_JOB_SITE, true, Optional.empty(), (l, pos) -> true)),
            p(7, new GoToPotentialJobSite(speedModifier)), p(8, YieldJobSite.create(speedModifier)),
            p(10, AcquirePoi.create(poi -> ((Holder) poi).is(PoiTypes.HOME), MemoryModuleType.HOME, false, Optional.of((byte) 14), VillagerGoalPackages::validateBedPoi)),
            p(10, AcquirePoi.create(poi -> ((Holder) poi).is(PoiTypes.MEETING), MemoryModuleType.MEETING_POINT, true, Optional.of((byte) 14))),
            p(10, AssignProfessionFromJobSite.create()), p(10, ResetProfession.create()));
    }

    private static boolean validateBedPoi(ServerLevel level, BlockPos pos) {
        var state = level.getBlockState(pos);
        return state.is(net.minecraft.tags.BlockTags.VILLAGERS_CAN_SLEEP_ON_BED) && !state.getValue(BedBlock.OCCUPIED);
    }

    public static ImmutableList<Pair<Integer, ? extends BehaviorControl<? super Villager>>> getWorkPackage(Holder<VillagerProfession> profession, float speedModifier) {
        WorkAtPoi workAtPoi = profession.is(VillagerProfession.FARMER) ? new WorkAtComposter() : new WorkAtPoi();
        boolean farmer = profession.is(VillagerProfession.FARMER);
        return pack(minimalLook(), p(5, new RunOne(weighted(wt(workAtPoi, 7), wt(StrollAroundPoi.create(MemoryModuleType.JOB_SITE, STROLL_SPEED_MODIFIER, 4), 2),
                wt(StrollToPoi.create(MemoryModuleType.JOB_SITE, STROLL_SPEED_MODIFIER, 1, 10), 5),
                wt(StrollToPoiList.create(MemoryModuleType.SECONDARY_JOB_SITE, speedModifier, 1, 6, MemoryModuleType.JOB_SITE), 5),
                wt(new HarvestFarmland(), farmer ? 2 : 5), wt(new UseBonemeal(), farmer ? 4 : 7)))),
            p(10, new ShowTradesToPlayer(400, 1600)), p(10, SetLookAndInteract.create(EntityTypes.PLAYER, 4)),
            p(2, SetWalkTargetFromBlockMemory.create(MemoryModuleType.JOB_SITE, speedModifier, 9, 100, 1200)), p(3, new GiveGiftToHero(100)),
            p(99, UpdateActivityFromSchedule.create()));
    }

    public static ImmutableList<Pair<Integer, ? extends BehaviorControl<? super Villager>>> getPlayPackage(float speedModifier) {
        return pack(p(0, new MoveToTargetSink(80, 120)), fullLook(), p(5, PlayTagWithOtherKids.create()),
            p(5, new RunOne(Map.of(MemoryModuleType.VISIBLE_VILLAGER_BABIES, MemoryStatus.VALUE_ABSENT),
                weighted(wt(InteractWith.of(EntityTypes.VILLAGER, 8, MemoryModuleType.INTERACTION_TARGET, speedModifier, 2), 2),
                    wt(InteractWith.of(EntityTypes.CAT, 8, MemoryModuleType.INTERACTION_TARGET, speedModifier, 2), 1), wt(VillageBoundRandomStroll.create(speedModifier), 1),
                    wt(SetWalkTargetFromLookTarget.create(speedModifier, 2), 1), wt(new JumpOnBed(speedModifier), 2), wt(new DoNothing(20, 40), 2)))),
            p(99, UpdateActivityFromSchedule.create()));
    }

    public static ImmutableList<Pair<Integer, ? extends BehaviorControl<? super Villager>>> getRestPackage(float speedModifier) {
        return pack(p(2, SetWalkTargetFromBlockMemory.create(MemoryModuleType.HOME, speedModifier, 1, 150, 1200)),
            p(3, ValidateNearbyPoi.create(poi -> ((Holder) poi).is(PoiTypes.HOME), MemoryModuleType.HOME)), p(3, new SleepInBed()),
            p(5, new RunOne(Map.of(MemoryModuleType.HOME, MemoryStatus.VALUE_ABSENT),
                weighted(wt(SetClosestHomeAsWalkTarget.create(speedModifier), 1), wt(InsideBrownianWalk.create(speedModifier), 4),
                    wt(GoToClosestVillage.create(speedModifier, 4), 2), wt(new DoNothing(20, 40), 2)))),
            minimalLook(), p(99, UpdateActivityFromSchedule.create()));
    }

    public static ImmutableList<Pair<Integer, ? extends BehaviorControl<? super Villager>>> getMeetPackage(float speedModifier) {
        return pack(p(2, TriggerGate.triggerOneShuffled(weighted(Pair.of(StrollAroundPoi.create(MemoryModuleType.MEETING_POINT, STROLL_SPEED_MODIFIER, 40), 2),
                Pair.of(SocializeAtBell.create(), 2)))),
            p(10, new ShowTradesToPlayer(400, 1600)), p(10, SetLookAndInteract.create(EntityTypes.PLAYER, 4)),
            p(2, SetWalkTargetFromBlockMemory.create(MemoryModuleType.MEETING_POINT, speedModifier, 6, 100, 200)), p(3, new GiveGiftToHero(100)),
            p(3, ValidateNearbyPoi.create(poi -> ((Holder) poi).is(PoiTypes.MEETING), MemoryModuleType.MEETING_POINT)),
            p(3, new GateBehavior(Map.of(), ImmutableSet.of(MemoryModuleType.INTERACTION_TARGET), GateBehavior.OrderPolicy.ORDERED, GateBehavior.RunningPolicy.RUN_ONE,
                weighted(wt(new TradeWithVillager(), 1)))),
            fullLook(), p(99, UpdateActivityFromSchedule.create()));
    }

    public static ImmutableList<Pair<Integer, ? extends BehaviorControl<? super Villager>>> getIdlePackage(float speedModifier) {
        return pack(p(2, new RunOne(weighted(wt(InteractWith.of(EntityTypes.VILLAGER, 8, MemoryModuleType.INTERACTION_TARGET, speedModifier, 2), 2),
                wt(InteractWith.of(EntityTypes.VILLAGER, 8, AgeableMob::canBreed, AgeableMob::canBreed, MemoryModuleType.BREED_TARGET, speedModifier, 2), 1),
                wt(InteractWith.of(EntityTypes.CAT, 8, MemoryModuleType.INTERACTION_TARGET, speedModifier, 2), 1), wt(VillageBoundRandomStroll.create(speedModifier), 1),
                wt(SetWalkTargetFromLookTarget.create(speedModifier, 2), 1), wt(new JumpOnBed(speedModifier), 1), wt(new DoNothing(30, 60), 1)))),
            p(3, new GiveGiftToHero(100)), p(3, SetLookAndInteract.create(EntityTypes.PLAYER, 4)), p(3, new ShowTradesToPlayer(400, 1600)),
            p(3, new GateBehavior(Map.of(), ImmutableSet.of(MemoryModuleType.INTERACTION_TARGET), GateBehavior.OrderPolicy.ORDERED, GateBehavior.RunningPolicy.RUN_ONE,
                weighted(wt(new TradeWithVillager(), 1)))),
            p(3, new GateBehavior(Map.of(), ImmutableSet.of(MemoryModuleType.BREED_TARGET), GateBehavior.OrderPolicy.ORDERED, GateBehavior.RunningPolicy.RUN_ONE,
                weighted(wt(new VillagerMakeLove(), 1)))),
            fullLook(), p(99, UpdateActivityFromSchedule.create()));
    }

    public static ImmutableList<Pair<Integer, ? extends BehaviorControl<? super Villager>>> getPanicPackage(float speedModifier) {
        float runawaySpeed = speedModifier * 1.5F;
        return pack(p(0, VillagerCalmDown.create()), p(1, SetWalkTargetAwayFrom.entity(MemoryModuleType.NEAREST_HOSTILE, runawaySpeed, 6, false)),
            p(1, SetWalkTargetAwayFrom.entity(MemoryModuleType.HURT_BY_ENTITY, runawaySpeed, 6, false)), p(3, VillageBoundRandomStroll.create(runawaySpeed, 2, 2)),
            minimalLook());
    }

    public static ImmutableList<Pair<Integer, ? extends BehaviorControl<? super Villager>>> getPreRaidPackage(float speedModifier) {
        return pack(p(0, RingBell.create()), p(0, TriggerGate.triggerOneShuffled(weighted(
                Pair.of(SetWalkTargetFromBlockMemory.create(MemoryModuleType.MEETING_POINT, speedModifier * 1.5F, 2, 150, 200), 6),
                Pair.of(VillageBoundRandomStroll.create(speedModifier * 1.5F), 2)))),
            minimalLook(), p(99, ResetRaidStatus.create()));
    }

    public static ImmutableList<Pair<Integer, ? extends BehaviorControl<? super Villager>>> getRaidPackage(float speedModifier) {
        OneShot celebrateOrStroll = TriggerGate.triggerOneShuffled(weighted(Pair.of(MoveToSkySeeingSpot.create(speedModifier), 5),
            Pair.of(VillageBoundRandomStroll.create(speedModifier * 1.1F), 2)));
        return pack(p(0, OneShots.<LivingEntity>of(java.util.Set.of(), (level, body, timestamp) -> raidExistsAndNotVictory(level, body)
                && celebrateOrStroll.trigger(level, body, timestamp))),
            p(0, new CelebrateVillagersSurvivedRaid(600, 600)),
            p(2, OneShots.<LivingEntity>of(java.util.Set.of(), (level, body, timestamp) -> raidExistsAndActive(level, body)
                && LocateHidingPlace.create(24, speedModifier * 1.4F, 1).trigger(level, body, timestamp))),
            minimalLook(), p(99, ResetRaidStatus.create()));
    }

    public static ImmutableList<Pair<Integer, ? extends BehaviorControl<? super Villager>>> getHidePackage(float speedModifier) {
        return pack(p(0, SetHiddenState.create(15, 3)), p(1, LocateHidingPlace.create(32, speedModifier * 1.25F, 2)), minimalLook());
    }

    private static Pair<Integer, BehaviorControl> fullLook() {
        return p(5, new RunOne(weighted(wt(SetEntityLookTarget.create(EntityTypes.CAT, 8.0F), 8), wt(SetEntityLookTarget.create(EntityTypes.VILLAGER, 8.0F), 2),
            wt(SetEntityLookTarget.create(EntityTypes.PLAYER, 8.0F), 2), wt(SetEntityLookTarget.create(MobCategory.CREATURE, 8.0F), 1),
            wt(SetEntityLookTarget.create(MobCategory.WATER_CREATURE, 8.0F), 1), wt(SetEntityLookTarget.create(MobCategory.AXOLOTLS, 8.0F), 1),
            wt(SetEntityLookTarget.create(MobCategory.UNDERGROUND_WATER_CREATURE, 8.0F), 1), wt(SetEntityLookTarget.create(MobCategory.WATER_AMBIENT, 8.0F), 1),
            wt(SetEntityLookTarget.create(MobCategory.MONSTER, 8.0F), 1), wt(new DoNothing(30, 60), 2))));
    }

    private static Pair<Integer, BehaviorControl> minimalLook() {
        return p(5, new RunOne(weighted(wt(SetEntityLookTarget.create(EntityTypes.VILLAGER, 8.0F), 2), wt(SetEntityLookTarget.create(EntityTypes.PLAYER, 8.0F), 2),
            wt(new DoNothing(30, 60), 8))));
    }

    private static boolean raidExistsAndActive(ServerLevel level, LivingEntity body) {
        var raid = level.getRaidAt(body.blockPosition());
        return raid != null && raid.isActive() && !raid.isVictory() && !raid.isLoss();
    }

    private static boolean raidExistsAndNotVictory(ServerLevel level, LivingEntity body) {
        var raid = level.getRaidAt(body.blockPosition());
        return raid != null && raid.isVictory();
    }
}
