// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.ai.navigation;

import net.minecraft.core.BlockPos;
import net.minecraft.world.entity.Entity;
import net.minecraft.world.entity.Mob;
import net.minecraft.world.level.Level;
import net.minecraft.world.level.pathfinder.Path;

public class WallClimberNavigation extends GroundPathNavigation {
    private BlockPos pathToPosition;

    public WallClimberNavigation(Mob mob, Level level) {
        super(mob, level);
    }

    @Override
    public Path createPath(BlockPos pos, int reachRange) {
        pathToPosition = pos;
        return super.createPath(pos, reachRange);
    }

    @Override
    public Path createPath(Entity target, int reachRange) {
        pathToPosition = target.blockPosition();
        return super.createPath(target, reachRange);
    }

    @Override
    public boolean moveTo(Entity target, double speedModifier) {
        Path newPath = createPath(target, 0);
        if (newPath != null) {
            return moveTo(newPath, speedModifier);
        }
        pathToPosition = target.blockPosition();
        this.speedModifier = speedModifier;
        return moveTo(target.getX(), target.getY(), target.getZ(), speedModifier);
    }
}
