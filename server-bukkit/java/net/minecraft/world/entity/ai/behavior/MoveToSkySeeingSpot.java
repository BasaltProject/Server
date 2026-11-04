// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.ai.behavior;

import java.util.List;
import java.util.Optional;
import java.util.Set;
import net.minecraft.core.BlockPos;
import net.minecraft.core.GlobalPos;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.world.entity.LivingEntity;
import net.minecraft.world.entity.PathfinderMob;
import net.minecraft.world.entity.ai.Brain;
import net.minecraft.world.entity.ai.memory.MemoryModuleType;
import net.minecraft.world.entity.ai.memory.WalkTarget;
import net.minecraft.world.entity.npc.villager.Villager;
import net.minecraft.world.entity.schedule.Activity;
import net.minecraft.world.phys.Vec3;

public class MoveToSkySeeingSpot {
    public static OneShot<LivingEntity> create(float speedModifier) {
        return OneShots.<LivingEntity>of(Set.of(MemoryModuleType.WALK_TARGET), (level, body, timestamp) -> {
            if (body.getBrain().hasMemoryValue(MemoryModuleType.WALK_TARGET) || level.canSeeSky(body.blockPosition())) {
                return false;
            }
            Vec3 pos = getOutdoorPosition(level, body);
            if (pos != null) {
                body.getBrain().setMemory(MemoryModuleType.WALK_TARGET, new WalkTarget(pos, speedModifier, 0));
            }
            return true;
        });
    }

    private static Vec3 getOutdoorPosition(ServerLevel level, LivingEntity body) {
        net.minecraft.util.RandomSource random = body.getRandom();
        BlockPos pos = body.blockPosition();
        for (int i = 0; i < 10; ++i) {
            BlockPos randomPos = pos.offset(random.nextInt(20) - 10, random.nextInt(6) - 3, random.nextInt(20) - 10);
            if (hasNoBlocksAbove(level, body, randomPos)) {
                return Vec3.atBottomCenterOf(randomPos);
            }
        }
        return null;
    }

    public static boolean hasNoBlocksAbove(ServerLevel level, LivingEntity body, BlockPos target) {
        return level.canSeeSky(target) && level.getHeightmapPos(net.minecraft.world.level.levelgen.Heightmap.Types.MOTION_BLOCKING, target).getY() <= body.getY();
    }
}
