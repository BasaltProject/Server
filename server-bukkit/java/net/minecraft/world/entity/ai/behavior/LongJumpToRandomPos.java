// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.ai.behavior;

import java.util.ArrayList;
import java.util.Collections;
import java.util.List;
import java.util.Map;
import java.util.Optional;
import java.util.function.BiPredicate;
import java.util.function.Function;
import net.minecraft.core.BlockPos;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.sounds.SoundEvent;
import net.minecraft.sounds.SoundSource;
import net.minecraft.util.Mth;
import net.minecraft.util.random.WeightedRandom;
import net.minecraft.util.valueproviders.UniformInt;
import net.minecraft.world.entity.Mob;
import net.minecraft.world.entity.ai.attributes.Attributes;
import net.minecraft.world.entity.ai.memory.MemoryModuleType;
import net.minecraft.world.entity.ai.memory.MemoryStatus;
import net.minecraft.world.entity.ai.navigation.PathNavigation;
import net.minecraft.world.level.Level;
import net.minecraft.world.level.block.Blocks;
import net.minecraft.world.level.pathfinder.Path;
import net.minecraft.world.level.pathfinder.WalkNodeEvaluator;
import net.minecraft.world.phys.Vec3;

public class LongJumpToRandomPos<E extends Mob> extends Behavior<E> {
    protected static final int FIND_JUMP_TRIES = 20;
    private static final int PREPARE_JUMP_DURATION = 40;
    protected static final int MIN_PATHFIND_DISTANCE_TO_VALID_JUMP = 8;
    private static final int TIME_OUT_DURATION = 200;
    private static final List<Integer> ALLOWED_ANGLES = List.of(65, 70, 75, 80);
    private final UniformInt timeBetweenLongJumps;
    protected final int maxLongJumpHeight;
    protected final int maxLongJumpWidth;
    protected final float maxJumpVelocityMultiplier;
    protected List<PossibleJump> jumpCandidates = new ArrayList<>();
    protected Optional<Vec3> initialPosition = Optional.empty();
    protected Vec3 chosenJump;
    protected int findJumpTries;
    protected long prepareJumpStart;
    private final Function<E, SoundEvent> getJumpSound;
    private final BiPredicate<E, BlockPos> acceptableLandingSpot;

    public LongJumpToRandomPos(UniformInt timeBetweenLongJumps, int maxLongJumpHeight, int maxLongJumpWidth, float maxJumpVelocityMultiplier, Function<E, SoundEvent> getJumpSound) {
        this(timeBetweenLongJumps, maxLongJumpHeight, maxLongJumpWidth, maxJumpVelocityMultiplier, getJumpSound, LongJumpToRandomPos::defaultAcceptableLandingSpot);
    }

    public static <E extends Mob> boolean defaultAcceptableLandingSpot(E body, BlockPos targetPos) {
        Level level = body.level();
        return level.getBlockState(targetPos.below()).isSolidRender() && body.getPathfindingMalus(WalkNodeEvaluator.getPathTypeStatic(body, targetPos)) == 0.0F;
    }

    public LongJumpToRandomPos(UniformInt timeBetweenLongJumps, int maxLongJumpHeight, int maxLongJumpWidth, float maxJumpVelocityMultiplier,
            Function<E, SoundEvent> getJumpSound, BiPredicate<E, BlockPos> acceptableLandingSpot) {
        super(Map.of(MemoryModuleType.LOOK_TARGET, MemoryStatus.REGISTERED, MemoryModuleType.LONG_JUMP_COOLDOWN_TICKS, MemoryStatus.VALUE_ABSENT,
            MemoryModuleType.LONG_JUMP_MID_JUMP, MemoryStatus.VALUE_ABSENT), TIME_OUT_DURATION);
        this.timeBetweenLongJumps = timeBetweenLongJumps;
        this.maxLongJumpHeight = maxLongJumpHeight;
        this.maxLongJumpWidth = maxLongJumpWidth;
        this.maxJumpVelocityMultiplier = maxJumpVelocityMultiplier;
        this.getJumpSound = getJumpSound;
        this.acceptableLandingSpot = acceptableLandingSpot;
    }

    protected boolean checkExtraStartConditions(ServerLevel level, Mob body) {
        boolean canStart = body.onGround() && !body.isInWater() && !body.isInLava() && !level.getBlockState(body.blockPosition()).is(Blocks.HONEY_BLOCK);
        if (!canStart) {
            body.getBrain().setMemory(MemoryModuleType.LONG_JUMP_COOLDOWN_TICKS, timeBetweenLongJumps.sample(level.getRandom()) / 2);
        }
        return canStart;
    }

    protected boolean canStillUse(ServerLevel level, Mob body, long timestamp) {
        boolean valid = initialPosition.isPresent() && initialPosition.get().equals(body.position()) && findJumpTries > 0 && !body.isInWater()
            && (chosenJump != null || !jumpCandidates.isEmpty());
        if (!valid && body.getBrain().getMemory(MemoryModuleType.LONG_JUMP_MID_JUMP).isEmpty()) {
            body.getBrain().setMemory(MemoryModuleType.LONG_JUMP_COOLDOWN_TICKS, timeBetweenLongJumps.sample(level.getRandom()) / 2);
            body.getBrain().eraseMemory(MemoryModuleType.LOOK_TARGET);
        }
        return valid;
    }

    protected void start(ServerLevel level, E body, long timestamp) {
        chosenJump = null;
        findJumpTries = FIND_JUMP_TRIES;
        initialPosition = Optional.of(body.position());
        BlockPos origin = body.blockPosition();
        jumpCandidates = new ArrayList<>();
        for (BlockPos pos : BlockPos.betweenClosed(origin.offset(-maxLongJumpWidth, -maxLongJumpHeight, -maxLongJumpWidth),
                origin.offset(maxLongJumpWidth, maxLongJumpHeight, maxLongJumpWidth))) {
            if (!pos.equals(origin)) {
                jumpCandidates.add(new PossibleJump(pos.immutable(), Mth.ceil(origin.distSqr(pos))));
            }
        }
    }

    protected void tick(ServerLevel level, E body, long timestamp) {
        if (chosenJump != null) {
            if (timestamp - prepareJumpStart >= PREPARE_JUMP_DURATION) {
                body.setYRot(body.yBodyRot);
                body.setDiscardFriction(true);
                double length = chosenJump.length();
                body.setDeltaMovement(chosenJump.scale((length + body.getJumpBoostPower()) / length));
                body.getBrain().setMemory(MemoryModuleType.LONG_JUMP_MID_JUMP, true);
                level.playSound(null, body, getJumpSound.apply(body), SoundSource.NEUTRAL, 1.0F, 1.0F);
            }
        } else {
            findJumpTries--;
            pickCandidate(level, body, timestamp);
        }
    }

    protected void pickCandidate(ServerLevel level, E body, long timestamp) {
        while (!jumpCandidates.isEmpty()) {
            Optional<PossibleJump> candidate = getJumpCandidate(level);
            if (candidate.isEmpty()) {
                continue;
            }
            BlockPos target = candidate.get().targetPos();
            if (!isAcceptableLandingPosition(body, target)) {
                continue;
            }
            Vec3 jump = calculateOptimalJumpVector(body, Vec3.atCenterOf(target));
            if (jump == null) {
                continue;
            }
            body.getBrain().setMemory(MemoryModuleType.LOOK_TARGET, new BlockPosTracker(target));
            PathNavigation navigation = body.getNavigation();
            Path path = navigation.createPath(target, 0, MIN_PATHFIND_DISTANCE_TO_VALID_JUMP);
            if (path != null && path.canReach()) {
                continue;
            }
            chosenJump = jump;
            prepareJumpStart = timestamp;
            return;
        }
    }

    protected Optional<PossibleJump> getJumpCandidate(ServerLevel level) {
        Optional<PossibleJump> picked = WeightedRandom.getRandomItem(level.getRandom(), jumpCandidates, PossibleJump::weight);
        picked.ifPresent(jumpCandidates::remove);
        return picked;
    }

    private boolean isAcceptableLandingPosition(E body, BlockPos target) {
        BlockPos at = body.blockPosition();
        return (at.getX() != target.getX() || at.getZ() != target.getZ()) && acceptableLandingSpot.test(body, target);
    }

    protected Vec3 calculateOptimalJumpVector(Mob body, Vec3 targetPos) {
        List<Integer> angles = new ArrayList<>(ALLOWED_ANGLES);
        Collections.shuffle(angles);
        float maxVelocity = (float) (body.getAttributeValue(Attributes.JUMP_STRENGTH) * maxJumpVelocityMultiplier);
        for (int angle : angles) {
            Optional<Vec3> velocity = LongJumpUtil.calculateJumpVectorForAngle(body, targetPos, maxVelocity, angle, true);
            if (velocity.isPresent()) {
                return velocity.get();
            }
        }
        return null;
    }

    public static record PossibleJump(BlockPos targetPos, int weight) {
    }
}
