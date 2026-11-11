// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.core.dispenser;
import java.util.List;
import net.minecraft.core.BlockPos;
import net.minecraft.core.Direction;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.world.entity.monster.cubemob.SulfurCube;
import net.minecraft.world.item.ItemStack;
import net.minecraft.world.level.block.BaseFireBlock;
import net.minecraft.world.level.block.CampfireBlock;
import net.minecraft.world.level.block.CandleBlock;
import net.minecraft.world.level.block.CandleCakeBlock;
import net.minecraft.world.level.block.DispenserBlock;
import net.minecraft.world.level.block.TntBlock;
import net.minecraft.world.level.block.state.BlockState;
import net.minecraft.world.level.block.state.properties.BlockStateProperties;
import net.minecraft.world.level.gameevent.GameEvent;
import net.minecraft.world.phys.AABB;
public class FlintAndSteelDispenseItemBehavior extends OptionalDispenseItemBehavior {
    protected ItemStack execute(BlockSource source, ItemStack dispensed) {
        ServerLevel level = source.level();
        setSuccess(true);
        Direction facing = source.state().getValue(DispenserBlock.FACING);
        BlockPos at = source.pos().relative(facing);
        BlockState target = level.getBlockState(at);
        if (!igniteSulfurCube(level, at)) {
            if (BaseFireBlock.canBePlacedAt(level, at, facing)) {
                level.setBlockAndUpdate(at, BaseFireBlock.getState(level, at));
                level.gameEvent(null, GameEvent.BLOCK_PLACE, at);
            } else if (CampfireBlock.canLight(target) || CandleBlock.canLight(target) || CandleCakeBlock.canLight(target)) {
                level.setBlockAndUpdate(at, target.setValue(BlockStateProperties.LIT, true));
                level.gameEvent(null, GameEvent.BLOCK_CHANGE, at);
            } else if (target.getBlock() instanceof TntBlock) {
                if (TntBlock.prime(level, at, null, dispensed)) {
                    level.removeBlock(at, false);
                } else {
                    setSuccess(false);
                }
            }
        }
        if (isSuccess()) {
            dispensed.hurtAndBreak(1, level, null, item -> {
            });
        }
        return dispensed;
    }
    private static boolean igniteSulfurCube(ServerLevel level, BlockPos pos) {
        List<SulfurCube> cubes = level.getEntitiesOfClass(SulfurCube.class, new AABB(pos), SulfurCube::canExplode);
        if (cubes.isEmpty()) {
            return false;
        }
        cubes.getFirst().primeTime(false);
        return true;
    }
}
