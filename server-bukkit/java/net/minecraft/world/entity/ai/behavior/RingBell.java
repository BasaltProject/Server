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

public class RingBell {
    private static final float BELL_RING_CHANCE = 0.95F;
    public static final int RING_BELL_FROM_DISTANCE = 3;

    public static BehaviorControl<LivingEntity> create() {
        return OneShots.<LivingEntity>of(Set.of(MemoryModuleType.MEETING_POINT), (level, body, timestamp) -> {
            GlobalPos meeting = body.getBrain().getMemory(MemoryModuleType.MEETING_POINT).orElse(null);
            if (meeting == null || level.getRandom().nextFloat() <= BELL_RING_CHANCE) {
                return false;
            }
            BlockPos pos = meeting.pos();
            if (pos.closerThan(body.blockPosition(), RING_BELL_FROM_DISTANCE)) {
                var state = level.getBlockState(pos);
                if (state.is(net.minecraft.world.level.block.Blocks.BELL)) {
                    if (level.getWorld().getBlockAt(pos.getX(), pos.getY(), pos.getZ()).getState() instanceof org.bukkit.block.Bell bell) {
                        bell.ring();
                    }
                }
            }
            return true;
        });
    }
}
