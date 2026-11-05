// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.damagesource;
import net.minecraft.world.entity.LivingEntity;
import net.minecraft.world.level.block.state.BlockState;
public record FallLocation(String id) {
    public static final FallLocation GENERIC = new FallLocation("generic");
    public static final FallLocation LADDER = new FallLocation("ladder");
    public static final FallLocation VINES = new FallLocation("vines");
    public static final FallLocation WEEPING_VINES = new FallLocation("weeping_vines");
    public static final FallLocation TWISTING_VINES = new FallLocation("twisting_vines");
    public static final FallLocation SCAFFOLDING = new FallLocation("scaffolding");
    public static final FallLocation OTHER_CLIMBABLE = new FallLocation("other_climbable");
    public static final FallLocation WATER = new FallLocation("water");
    public static FallLocation blockToFallLocation(BlockState blockState) {
        if (blockState.is(net.minecraft.world.level.block.Blocks.LADDER) || blockState.is(net.minecraft.tags.BlockTags.TRAPDOORS)) {
            return LADDER;
        }
        if (blockState.is(net.minecraft.world.level.block.Blocks.VINE)) {
            return VINES;
        }
        if (blockState.is(net.minecraft.world.level.block.Blocks.WEEPING_VINES) || blockState.is(net.minecraft.world.level.block.Blocks.WEEPING_VINES_PLANT)) {
            return WEEPING_VINES;
        }
        if (blockState.is(net.minecraft.world.level.block.Blocks.TWISTING_VINES) || blockState.is(net.minecraft.world.level.block.Blocks.TWISTING_VINES_PLANT)) {
            return TWISTING_VINES;
        }
        return blockState.is(net.minecraft.world.level.block.Blocks.SCAFFOLDING) ? SCAFFOLDING : OTHER_CLIMBABLE;
    }
    public static FallLocation getCurrentFallLocation(LivingEntity mob) {
        if (mob.onClimbable()) {
            return blockToFallLocation(mob.getInBlockState());
        }
        return mob.isInWater() ? WATER : GENERIC;
    }
    public String languageKey() {
        return "death.fell.accident." + id;
    }
}
