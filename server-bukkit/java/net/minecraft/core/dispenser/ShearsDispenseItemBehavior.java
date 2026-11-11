// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.core.dispenser;
import net.minecraft.core.BlockPos;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.sounds.SoundEvents;
import net.minecraft.sounds.SoundSource;
import net.minecraft.tags.BlockTags;
import net.minecraft.world.entity.Entity;
import net.minecraft.world.entity.EntitySelector;
import net.minecraft.world.entity.Shearable;
import net.minecraft.world.item.ItemStack;
import net.minecraft.world.level.block.BeehiveBlock;
import net.minecraft.world.level.block.Block;
import net.minecraft.world.level.storage.loot.BuiltInLootTables;
import net.minecraft.world.level.block.DispenserBlock;
import net.minecraft.world.level.block.entity.BeehiveBlockEntity;
import net.minecraft.world.level.block.state.BlockState;
import net.minecraft.world.level.gameevent.GameEvent;
import net.minecraft.world.phys.AABB;
public class ShearsDispenseItemBehavior extends OptionalDispenseItemBehavior {
    protected ItemStack execute(BlockSource source, ItemStack dispensed) {
        ServerLevel level = source.level();
        if (level.isClientSide()) {
            return dispensed;
        }
        BlockPos front = source.pos().relative(source.state().getValue(DispenserBlock.FACING));
        setSuccess(shearHive(level, dispensed, front) || shearEntity(level, front, dispensed));
        if (isSuccess()) {
            dispensed.hurtAndBreak(1, level, null, item -> {
            });
        }
        return dispensed;
    }
    private static boolean shearHive(ServerLevel level, ItemStack tool, BlockPos pos) {
        BlockState state = level.getBlockState(pos);
        if (!state.is(BlockTags.BEEHIVES, s -> s.hasProperty(BeehiveBlock.HONEY_LEVEL) && s.getBlock() instanceof BeehiveBlock)) {
            return false;
        }
        if (state.getValue(BeehiveBlock.HONEY_LEVEL) < 5) {
            return false;
        }
        level.playSound(null, pos, SoundEvents.BEEHIVE_SHEAR, SoundSource.BLOCKS, 1.0F, 1.0F);
        Block.dropFromBlockInteractLootTable(level, BuiltInLootTables.HARVEST_BEEHIVE, pos, state, level.getBlockEntity(pos), tool, null,
                (sl, stack) -> Block.popResource(sl, pos, stack));
        level.setBlockAndUpdate(pos, state.setValue(BeehiveBlock.HONEY_LEVEL, 0));
        if (level.getBlockEntity(pos) instanceof BeehiveBlockEntity hive) {
            hive.emptyAllLivingFromHive(null, BeehiveBlockEntity.BeeReleaseStatus.BEE_RELEASED);
        }
        level.gameEvent(null, GameEvent.SHEAR, pos);
        return true;
    }
    private static boolean shearEntity(ServerLevel level, BlockPos pos, ItemStack tool) {
        for (Entity entity : level.getEntitiesOfClass(Entity.class, new AABB(pos), EntitySelector.NO_SPECTATORS)) {
            if (entity.shearOffAllLeashConnections(null)) {
                return true;
            }
            if (entity.isAlive() && entity instanceof Shearable shearable && shearable.readyForShearing()) {
                shearable.shear(level, SoundSource.BLOCKS, tool);
                level.gameEvent(null, GameEvent.SHEAR, pos);
                return true;
            }
        }
        return false;
    }
}
