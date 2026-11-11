// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.ai.behavior;
import net.minecraft.world.level.pathfinder.WalkNodeEvaluator;
import net.minecraft.world.entity.ai.attributes.Attributes;
import java.util.Collections;
import java.util.Map;
import java.util.ArrayList;
import java.util.List;
import java.util.Optional;
import java.util.function.BiPredicate;
import java.util.function.Function;
import net.minecraft.core.BlockPos;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.sounds.SoundEvent;
import net.minecraft.util.valueproviders.UniformInt;
import net.minecraft.world.entity.Mob;
import net.minecraft.world.phys.Vec3;
public class LongJumpToRandomPos<E extends Mob> extends Behavior<E> {
    protected static final int FIND_JUMP_TRIES = 20;
    protected static final int MIN_PATHFIND_DISTANCE_TO_VALID_JUMP = 8;
    protected final int maxLongJumpHeight = 0;
    protected final int maxLongJumpWidth = 0;
    protected final float maxJumpVelocityMultiplier = 0;
    protected List<PossibleJump> jumpCandidates;
    protected Optional<Vec3> initialPosition;
    protected Vec3 chosenJump;
    protected int findJumpTries;
    protected long prepareJumpStart;
    public LongJumpToRandomPos(UniformInt timeBetweenLongJumps, int maxLongJumpHeight, int maxLongJumpWidth, float maxJumpVelocityMultiplier, Function<E, SoundEvent> getJumpSound) {
        super((java.util.Map) null);
    }
    public static <E extends Mob> boolean defaultAcceptableLandingSpot(E body, BlockPos targetPos) {
        BlockPos below = targetPos.below();
        return body.level().getBlockState(below).isSolidRender() && body.getPathfindingMalus(WalkNodeEvaluator.getPathTypeStatic(body, targetPos)) == 0.0F;
    }
    public LongJumpToRandomPos(UniformInt timeBetweenLongJumps, int maxLongJumpHeight, int maxLongJumpWidth, float maxJumpVelocityMultiplier, Function<E, SoundEvent> getJumpSound, BiPredicate<E, BlockPos> acceptableLandingSpot) {
        super((java.util.Map) null);
    }
    protected boolean checkExtraStartConditions(ServerLevel level, Mob body) {
        return false;
    }
    protected boolean canStillUse(ServerLevel level, Mob body, long timestamp) {
        return false;
    }
    protected void start(ServerLevel level, E body, long timestamp) {
        
    }
    protected void tick(ServerLevel level, E body, long timestamp) {
        
    }
    protected void pickCandidate(ServerLevel level, E body, long timestamp) {
        return;
    }
    protected Optional<PossibleJump> getJumpCandidate(ServerLevel level) {
        return Optional.empty();
    }
    protected Vec3 calculateOptimalJumpVector(Mob body, Vec3 targetPos) {
        List<Integer> angles = new ArrayList<>(List.of(65, 70, 75, 80));
        Collections.shuffle(angles);
        float limit = (float) (body.getAttributeValue(Attributes.JUMP_STRENGTH) * maxJumpVelocityMultiplier);
        for (int angle : angles) {
            Optional<Vec3> found = LongJumpUtil.calculateJumpVectorForAngle(body, targetPos, limit, angle, true);
            if (found.isPresent()) {
                return found.get();
            }
        }
        return null;

    }
    public static record PossibleJump(BlockPos targetPos, int weight) {
    }
}
