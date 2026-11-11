// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.block;
import net.minecraft.world.phys.Vec3;
import net.minecraft.core.BlockPos;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.util.RandomSource;
import net.minecraft.world.entity.Entity;
import net.minecraft.world.entity.InsideBlockEffectApplier;
import net.minecraft.world.item.ItemStack;
import net.minecraft.world.level.Level;
import net.minecraft.world.level.LevelReader;
import net.minecraft.world.level.block.entity.BlockEntity;
import net.minecraft.world.level.block.entity.BlockEntityTicker;
import net.minecraft.world.level.block.entity.BlockEntityType;
import net.minecraft.world.level.block.state.BlockBehaviour;
import net.minecraft.world.level.block.state.BlockState;
import net.minecraft.world.level.material.Fluid;
import net.minecraft.world.level.portal.TeleportTransition;
public class EndGatewayBlock extends BaseEntityBlock implements Portal {
    protected EndGatewayBlock(BlockBehaviour.Properties properties) {
        super((int) 0);
    }
    public BlockEntity newBlockEntity(BlockPos worldPosition, BlockState blockState) {
        return new net.minecraft.world.level.block.entity.TheEndGatewayBlockEntity(worldPosition, blockState);
    }
    public <T extends BlockEntity> BlockEntityTicker<T> getTicker(Level level, BlockState blockState, BlockEntityType<T> type) {
        return null;
    }
    public void animateTick(BlockState state, Level level, BlockPos pos, RandomSource random) {
        super.animateTick(state, level, pos, random);
    }
    protected ItemStack getCloneItemStack(LevelReader level, BlockPos pos, BlockState state, boolean includeData) {
        return super.getCloneItemStack(level, pos, state, includeData);
    }
    protected boolean canBeReplaced(BlockState state, Fluid fluid) {
        return super.canBeReplaced(state, fluid);
    }
    protected void entityInside(BlockState state, Level level, BlockPos pos, Entity entity, InsideBlockEffectApplier effectApplier, boolean isPrecise) {
        super.entityInside(state, level, pos, entity, effectApplier, isPrecise);
    }
    public TeleportTransition getPortalDestination(ServerLevel currentLevel, Entity entity, BlockPos portalEntryPos) {
        if (!(currentLevel.getBlockEntity(portalEntryPos) instanceof net.minecraft.world.level.block.entity.TheEndGatewayBlockEntity gateway)) {
            return null;
        }
        Vec3 destination = gateway.getPortalPosition(currentLevel, portalEntryPos);
        if (destination == null) {
            return null;
        }
        java.util.Set<net.minecraft.world.entity.Relative> relatives = entity instanceof net.minecraft.world.entity.projectile.throwableitemprojectile.ThrownEnderpearl
            ? java.util.Set.of() : net.minecraft.world.entity.Relative.union(net.minecraft.world.entity.Relative.DELTA, net.minecraft.world.entity.Relative.ROTATION);
        return new TeleportTransition(currentLevel, destination, Vec3.ZERO, 0.0F, 0.0F, relatives, TeleportTransition.PLACE_PORTAL_TICKET);
    }
    protected RenderShape getRenderShape(BlockState state) {
        return super.getRenderShape(state);
    }
}
