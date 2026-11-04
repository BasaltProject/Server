// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.ai.behavior;

import java.util.Map;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.world.entity.Mob;
import net.minecraft.world.entity.ai.memory.MemoryModuleType;
import net.minecraft.world.entity.ai.memory.MemoryStatus;
import net.minecraft.world.entity.ai.memory.WalkTarget;
import net.minecraft.world.level.pathfinder.Path;

public class MoveToTargetSink extends Behavior<Mob> {
    private Path path;
    private net.minecraft.core.BlockPos lastTargetPos;
    private float speedModifier;

    public MoveToTargetSink() {
        this(150, 250);
    }

    public MoveToTargetSink(int minTimeout, int maxTimeout) {
        super(Map.of(MemoryModuleType.CANT_REACH_WALK_TARGET_SINCE, MemoryStatus.VALUE_ABSENT, MemoryModuleType.PATH, MemoryStatus.REGISTERED,
            MemoryModuleType.WALK_TARGET, MemoryStatus.VALUE_PRESENT), minTimeout, maxTimeout);
    }

    private static boolean reached(Mob body, WalkTarget target) {
        return target.getTarget().currentBlockPosition().distManhattan(body.blockPosition()) <= target.getCloseEnoughDist();
    }

    protected boolean checkExtraStartConditions(ServerLevel level, Mob body) {
        var brain = body.getBrain();
        WalkTarget target = brain.getMemory(MemoryModuleType.WALK_TARGET).get();
        if (reached(body, target)) {
            brain.eraseMemory(MemoryModuleType.WALK_TARGET);
            return false;
        }
        Path found = body.getNavigation().createPath(target.getTarget().currentBlockPosition(), target.getCloseEnoughDist());
        if (found == null || !found.canReach()) {
            brain.setMemory(MemoryModuleType.CANT_REACH_WALK_TARGET_SINCE, level.getGameTime());
            return false;
        }
        path = found;
        speedModifier = target.getSpeedModifier();
        lastTargetPos = target.getTarget().currentBlockPosition();
        return true;
    }

    protected boolean canStillUse(ServerLevel level, Mob body, long timestamp) {
        if (path == null || lastTargetPos == null || body.getNavigation().isDone()) {
            return false;
        }
        return body.getBrain().getMemory(MemoryModuleType.WALK_TARGET).filter(target -> !reached(body, target)).isPresent();
    }

    protected void start(ServerLevel level, Mob body, long timestamp) {
        body.getBrain().setMemory(MemoryModuleType.PATH, path);
        body.getNavigation().moveTo(path, speedModifier);
    }

    protected void tick(ServerLevel level, Mob body, long timestamp) {
        Path current = body.getNavigation().getPath();
        if (current != path) {
            path = current;
            if (current != null) {
                body.getBrain().setMemory(MemoryModuleType.PATH, current);
            }
        }
    }

    protected void stop(ServerLevel level, Mob body, long timestamp) {
        var brain = body.getBrain();
        if (body.getNavigation().isDone() || brain.getMemory(MemoryModuleType.WALK_TARGET).filter(target -> reached(body, target)).isPresent()) {
            brain.eraseMemory(MemoryModuleType.WALK_TARGET);
        }
        brain.eraseMemory(MemoryModuleType.PATH);
        body.getNavigation().stop();
        path = null;
    }
}
