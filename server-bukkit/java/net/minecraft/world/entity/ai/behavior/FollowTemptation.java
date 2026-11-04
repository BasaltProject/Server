// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.ai.behavior;

import java.util.Map;
import java.util.Optional;
import java.util.function.Function;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.world.entity.LivingEntity;
import net.minecraft.world.entity.PathfinderMob;
import net.minecraft.world.entity.ai.memory.MemoryModuleType;
import net.minecraft.world.entity.ai.memory.MemoryStatus;
import net.minecraft.world.entity.ai.memory.WalkTarget;
import net.minecraft.world.entity.player.Player;

public class FollowTemptation extends Behavior<PathfinderMob> {
    public static final int TEMPTATION_COOLDOWN = 100;
    public static final double DEFAULT_CLOSE_ENOUGH_DIST = 2.5;
    public static final double BACKED_UP_CLOSE_ENOUGH_DIST = 3.5;
    private final Function<LivingEntity, Float> speedModifier;
    private final Function<LivingEntity, Double> closeEnoughDistance;
    private final boolean lookInTheEyes;

    public FollowTemptation(Function<LivingEntity, Float> speedModifier) {
        this(speedModifier, mob -> DEFAULT_CLOSE_ENOUGH_DIST);
    }

    public FollowTemptation(Function<LivingEntity, Float> speedModifier, Function<LivingEntity, Double> closeEnoughDistance) {
        this(speedModifier, closeEnoughDistance, false);
    }

    public FollowTemptation(Function<LivingEntity, Float> speedModifier, Function<LivingEntity, Double> closeEnoughDistance, boolean lookInTheEyes) {
        super(Map.of(MemoryModuleType.LOOK_TARGET, MemoryStatus.REGISTERED, MemoryModuleType.WALK_TARGET, MemoryStatus.REGISTERED,
            MemoryModuleType.TEMPTATION_COOLDOWN_TICKS, MemoryStatus.VALUE_ABSENT,
            MemoryModuleType.TEMPTING_PLAYER, MemoryStatus.VALUE_PRESENT));
        this.speedModifier = speedModifier;
        this.closeEnoughDistance = closeEnoughDistance;
        this.lookInTheEyes = lookInTheEyes;
    }

    protected float getSpeedModifier(PathfinderMob body) {
        return speedModifier.apply(body);
    }

    private Optional<Player> temptingPlayer(PathfinderMob body) {
        return body.getBrain().getMemory(MemoryModuleType.TEMPTING_PLAYER);
    }

    protected boolean timedOut(long timestamp) {
        return false;
    }

    protected boolean canStillUse(ServerLevel level, PathfinderMob body, long timestamp) {
        return temptingPlayer(body).isPresent() && !body.getBrain().hasMemoryValue(MemoryModuleType.TEMPTATION_COOLDOWN_TICKS);
    }

    protected void start(ServerLevel level, PathfinderMob body, long timestamp) {
        return;
    }

    protected void stop(ServerLevel level, PathfinderMob body, long timestamp) {
        var brain = body.getBrain();
        brain.setMemory(MemoryModuleType.TEMPTATION_COOLDOWN_TICKS, TEMPTATION_COOLDOWN);
        brain.eraseMemory(MemoryModuleType.WALK_TARGET);
        brain.eraseMemory(MemoryModuleType.LOOK_TARGET);
    }

    protected void tick(ServerLevel level, PathfinderMob body, long timestamp) {
        Player player = temptingPlayer(body).orElse(null);
        if (player == null) {
            return;
        }
        var brain = body.getBrain();
        brain.setMemory(MemoryModuleType.LOOK_TARGET, new EntityTracker(player, true, lookInTheEyes));
        if (body.distanceToSqr(player) < Math.pow(closeEnoughDistance.apply(body), 2)) {
            brain.eraseMemory(MemoryModuleType.WALK_TARGET);
        } else {
            brain.setMemory(MemoryModuleType.WALK_TARGET, new WalkTarget(new EntityTracker(player, false, lookInTheEyes), getSpeedModifier(body), 2));
        }
    }
}
