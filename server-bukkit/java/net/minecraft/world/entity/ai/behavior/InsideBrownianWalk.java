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

public class InsideBrownianWalk {
    public static BehaviorControl<PathfinderMob> create(float speedModifier) {
        return OneShots.<PathfinderMob>of(Set.of(MemoryModuleType.WALK_TARGET), (level, body, timestamp) -> {
            if (body.getBrain().hasMemoryValue(MemoryModuleType.WALK_TARGET) || level.canSeeSky(body.blockPosition())) {
                return false;
            }
            BlockPos bodyPos = body.blockPosition();
            List<BlockPos> poses = new java.util.ArrayList<>();
            for (BlockPos p : BlockPos.betweenClosed(bodyPos.offset(-1, -1, -1), bodyPos.offset(1, 1, 1))) {
                poses.add(p.immutable());
            }
            java.util.Collections.shuffle(poses);
            poses.stream().filter(pos -> !level.canSeeSky(pos)).filter(pos -> level.loadedAndEntityCanStandOn(pos, body)).filter(pos -> level.noCollision(body)).findFirst()
                .ifPresent(target -> body.getBrain().setMemory(MemoryModuleType.WALK_TARGET, new WalkTarget(target, speedModifier, 0)));
            return true;
        });
    }
}
