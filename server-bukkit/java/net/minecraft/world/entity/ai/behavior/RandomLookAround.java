// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.ai.behavior;

import java.util.Map;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.util.Mth;
import net.minecraft.util.valueproviders.IntProvider;
import net.minecraft.world.entity.Mob;
import net.minecraft.world.entity.ai.memory.MemoryModuleType;
import net.minecraft.world.entity.ai.memory.MemoryStatus;
import net.minecraft.world.phys.Vec3;

public class RandomLookAround extends Behavior<Mob> {
    private final IntProvider interval;
    private final float maxYaw;
    private final float minPitch;
    private final float maxPitch;

    public RandomLookAround(IntProvider interval, float maxYaw, float minPitch, float maxPitch) {
        super(Map.of(MemoryModuleType.LOOK_TARGET, MemoryStatus.VALUE_ABSENT, MemoryModuleType.GAZE_COOLDOWN_TICKS, MemoryStatus.VALUE_ABSENT));
        this.interval = interval;
        this.maxYaw = maxYaw;
        this.minPitch = minPitch;
        this.maxPitch = maxPitch;
    }

    protected void start(ServerLevel level, Mob body, long timestamp) {
        float yaw = Mth.clamp(body.getRandom().nextFloat(), 0.0F, 1.0F) * 2.0F * maxYaw - maxYaw;
        float pitch = minPitch + body.getRandom().nextFloat() * (maxPitch - minPitch);
        Vec3 look = Vec3.directionFromRotation(pitch, body.getYHeadRot() + yaw);
        body.getBrain().setMemory(MemoryModuleType.LOOK_TARGET, new BlockPosTracker(net.minecraft.core.BlockPos.containing(body.getEyePosition().add(look))));
        body.getBrain().setMemory(MemoryModuleType.GAZE_COOLDOWN_TICKS, interval.sample(body.getRandom()));
    }
}
