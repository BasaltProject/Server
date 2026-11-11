// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.ai.behavior;
import net.minecraft.world.level.levelgen.Heightmap;
import net.minecraft.core.BlockPos;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.world.entity.LivingEntity;
public class MoveToSkySeeingSpot {
    public static OneShot<LivingEntity> create(float speedModifier) {
        return new net.minecraft.world.entity.ai.behavior.InertBehavior<>();
    }
    public static boolean hasNoBlocksAbove(ServerLevel level, LivingEntity body, BlockPos target) {
        return level.canSeeSky(target) && level.getHeightmapPos(Heightmap.Types.MOTION_BLOCKING, target).getY() <= body.getY();
    }
}
