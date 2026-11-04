// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.ai.goal;

import java.util.EnumSet;
import net.minecraft.core.BlockPos;
import net.minecraft.tags.EntityTypeTags;
import net.minecraft.world.entity.Mob;
import net.minecraft.world.level.Level;
import net.minecraft.world.level.block.Blocks;
import net.minecraft.world.level.block.state.BlockState;
import net.minecraft.world.phys.shapes.Shapes;

public class ClimbOnTopOfPowderSnowGoal extends Goal {
    private final Mob mob;
    private final Level level;

    public ClimbOnTopOfPowderSnowGoal(Mob mob, Level level) {
        this.mob = mob;
        this.level = level;
        setFlags(EnumSet.of(Goal.Flag.JUMP));
    }

    @Override
    public boolean canUse() {
        if (!(mob.wasInPowderSnow || mob.isInPowderSnow) || !mob.is(EntityTypeTags.POWDER_SNOW_WALKABLE_MOBS)) {
            return false;
        }
        BlockPos above = mob.blockPosition().above();
        BlockState state = level.getBlockState(above);
        return state.is(Blocks.POWDER_SNOW) || state.getCollisionShape(level, above) == Shapes.empty();
    }

    @Override
    public boolean requiresUpdateEveryTick() {
        return true;
    }

    @Override
    public void tick() {
        mob.getJumpControl().jump();
    }
}
