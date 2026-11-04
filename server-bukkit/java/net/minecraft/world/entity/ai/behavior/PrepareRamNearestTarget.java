// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.ai.behavior;

import java.util.ArrayList;
import java.util.Comparator;
import java.util.List;
import java.util.Map;
import java.util.Optional;
import java.util.function.Function;
import java.util.function.ToIntFunction;
import net.minecraft.core.BlockPos;
import net.minecraft.core.Direction;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.sounds.SoundEvent;
import net.minecraft.sounds.SoundSource;
import net.minecraft.util.Mth;
import net.minecraft.world.entity.LivingEntity;
import net.minecraft.world.entity.PathfinderMob;
import net.minecraft.world.entity.ai.memory.MemoryModuleType;
import net.minecraft.world.entity.ai.memory.MemoryStatus;
import net.minecraft.world.entity.ai.memory.WalkTarget;
import net.minecraft.world.entity.ai.navigation.PathNavigation;
import net.minecraft.world.entity.ai.targeting.TargetingConditions;
import net.minecraft.world.level.pathfinder.Path;
import net.minecraft.world.level.pathfinder.WalkNodeEvaluator;
import net.minecraft.world.phys.Vec3;

public class PrepareRamNearestTarget<E extends PathfinderMob> extends Behavior<E> {
    public static final int TIME_OUT_DURATION = 160;
    private final ToIntFunction<E> getCooldownOnFail;
    private final int minRamDistance;
    private final int maxRamDistance;
    private final float walkSpeed;
    private final TargetingConditions ramTargeting;
    private final int ramPrepareTime;
    private final Function<E, SoundEvent> getPrepareRamSound;
    private Optional<Long> reachedRamPositionTimestamp = Optional.empty();
    private Optional<RamCandidate> ramCandidate = Optional.empty();

    public PrepareRamNearestTarget(ToIntFunction<E> getCooldownOnFail, int minRamDistance, int maxRamDistance, float walkSpeed,
            TargetingConditions ramTargeting, int ramPrepareTime, Function<E, SoundEvent> getPrepareRamSound) {
        super(Map.of(MemoryModuleType.LOOK_TARGET, MemoryStatus.REGISTERED, MemoryModuleType.RAM_COOLDOWN_TICKS, MemoryStatus.VALUE_ABSENT,
            MemoryModuleType.NEAREST_VISIBLE_LIVING_ENTITIES, MemoryStatus.VALUE_PRESENT, MemoryModuleType.RAM_TARGET, MemoryStatus.VALUE_ABSENT), TIME_OUT_DURATION);
        this.getCooldownOnFail = getCooldownOnFail;
        this.minRamDistance = minRamDistance;
        this.maxRamDistance = maxRamDistance;
        this.walkSpeed = walkSpeed;
        this.ramTargeting = ramTargeting;
        this.ramPrepareTime = ramPrepareTime;
        this.getPrepareRamSound = getPrepareRamSound;
    }

    protected void start(ServerLevel level, PathfinderMob body, long timestamp) {
        body.getBrain().getMemory(MemoryModuleType.NEAREST_VISIBLE_LIVING_ENTITIES)
            .flatMap(visible -> visible.findClosest(entity -> ramTargeting.test(level, body, entity)))
            .ifPresent(target -> chooseRamPosition(body, target));
    }

    protected void stop(ServerLevel level, E body, long timestamp) {
        var brain = body.getBrain();
        if (!brain.hasMemoryValue(MemoryModuleType.RAM_TARGET)) {
            level.broadcastEntityEvent(body, (byte) 59);
            brain.setMemory(MemoryModuleType.RAM_COOLDOWN_TICKS, getCooldownOnFail.applyAsInt(body));
        }
    }

    protected boolean canStillUse(ServerLevel level, PathfinderMob body, long timestamp) {
        return ramCandidate.isPresent() && ramCandidate.get().getTarget().isAlive();
    }

    protected void tick(ServerLevel level, E body, long timestamp) {
        if (ramCandidate.isEmpty()) {
            return;
        }
        RamCandidate candidate = ramCandidate.get();
        body.getBrain().setMemory(MemoryModuleType.WALK_TARGET, new WalkTarget(candidate.getStartPosition(), walkSpeed, 0));
        body.getBrain().setMemory(MemoryModuleType.LOOK_TARGET, new EntityTracker(candidate.getTarget(), true));
        if (!candidate.getTarget().blockPosition().equals(candidate.getTargetPosition())) {
            level.broadcastEntityEvent(body, (byte) 59);
            body.getNavigation().stop();
            chooseRamPosition(body, candidate.target);
            return;
        }
        BlockPos here = body.blockPosition();
        if (here.equals(candidate.getStartPosition())) {
            level.broadcastEntityEvent(body, (byte) 58);
            if (reachedRamPositionTimestamp.isEmpty()) {
                reachedRamPositionTimestamp = Optional.of(timestamp);
            }
            if (timestamp - reachedRamPositionTimestamp.get() >= ramPrepareTime) {
                body.getBrain().setMemory(MemoryModuleType.RAM_TARGET, edgeOfBlock(here, candidate.getTargetPosition()));
                level.playSound(null, body, getPrepareRamSound.apply(body), SoundSource.NEUTRAL, 1.0F, body.getVoicePitch());
                ramCandidate = Optional.empty();
            }
        }
    }

    private Vec3 edgeOfBlock(BlockPos start, BlockPos target) {
        double dx = 0.5 * Mth.sign(target.getX() - start.getX());
        double dz = 0.5 * Mth.sign(target.getZ() - start.getZ());
        return Vec3.atBottomCenterOf(target).add(dx, 0.0, dz);
    }

    private Optional<BlockPos> calculateRammingStartPosition(PathfinderMob body, LivingEntity target) {
        BlockPos targetPos = target.blockPosition();
        if (!isWalkableBlock(body, targetPos)) {
            return Optional.empty();
        }
        List<BlockPos> candidates = new ArrayList<>();
        BlockPos.MutableBlockPos furthest = targetPos.mutable();
        for (Direction direction : Direction.Plane.HORIZONTAL) {
            furthest.set(targetPos);
            for (int distance = 0; distance < maxRamDistance; distance++) {
                if (!isWalkableBlock(body, furthest.move(direction))) {
                    furthest.move(direction.getOpposite());
                    break;
                }
            }
            if (furthest.distManhattan(targetPos) >= minRamDistance) {
                candidates.add(furthest.immutable());
            }
        }
        PathNavigation navigation = body.getNavigation();
        BlockPos from = body.blockPosition();
        return candidates.stream().sorted(Comparator.comparingDouble(from::distSqr)).filter(pos -> {
            Path path = navigation.createPath(pos, 0);
            return path != null && path.canReach();
        }).findFirst();
    }

    private boolean isWalkableBlock(PathfinderMob body, BlockPos pos) {
        return body.getNavigation().isStableDestination(pos) && body.getPathfindingMalus(WalkNodeEvaluator.getPathTypeStatic(body, pos)) == 0.0F;
    }

    private void chooseRamPosition(PathfinderMob body, LivingEntity target) {
        reachedRamPositionTimestamp = Optional.empty();
        ramCandidate = calculateRammingStartPosition(body, target).map(pos -> new RamCandidate(pos, target.blockPosition(), target));
    }

    public static class RamCandidate {
        private final BlockPos startPosition;
        private final BlockPos targetPosition;
        private final LivingEntity target;

        public RamCandidate(BlockPos startPosition, BlockPos targetPosition, LivingEntity target) {
            this.startPosition = startPosition;
            this.targetPosition = targetPosition;
            this.target = target;
        }

        public BlockPos getStartPosition() {
            return startPosition;
        }

        public BlockPos getTargetPosition() {
            return targetPosition;
        }

        public LivingEntity getTarget() {
            return target;
        }
    }
}
