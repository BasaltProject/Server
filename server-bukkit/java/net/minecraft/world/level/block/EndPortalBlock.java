// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.block;
import net.minecraft.world.phys.Vec3;
import net.minecraft.core.Direction;
import net.minecraft.core.BlockPos;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.util.RandomSource;
import net.minecraft.world.entity.Entity;
import net.minecraft.world.entity.InsideBlockEffectApplier;
import net.minecraft.world.item.ItemStack;
import net.minecraft.world.level.BlockGetter;
import net.minecraft.world.level.Level;
import net.minecraft.world.level.LevelReader;
import net.minecraft.world.level.block.entity.BlockEntity;
import net.minecraft.world.level.block.state.BlockBehaviour;
import net.minecraft.world.level.block.state.BlockState;
import net.minecraft.world.level.material.Fluid;
import net.minecraft.world.level.portal.TeleportTransition;
import net.minecraft.world.phys.shapes.CollisionContext;
import net.minecraft.world.phys.shapes.VoxelShape;
public class EndPortalBlock extends BaseEntityBlock implements Portal {
    protected EndPortalBlock(BlockBehaviour.Properties properties) {
        super((int) 0);
    }
    public BlockEntity newBlockEntity(BlockPos worldPosition, BlockState blockState) {
        return new net.minecraft.world.level.block.entity.TheEndPortalBlockEntity(worldPosition, blockState);
    }
    protected VoxelShape getShape(BlockState state, BlockGetter level, BlockPos pos, CollisionContext context) {
        return super.getShape(state, level, pos, context);
    }
    protected VoxelShape getEntityInsideCollisionShape(BlockState state, BlockGetter level, BlockPos pos, Entity entity) {
        return super.getEntityInsideCollisionShape(state, level, pos, entity);
    }
    protected void entityInside(BlockState state, Level level, BlockPos pos, Entity entity, InsideBlockEffectApplier effectApplier, boolean isPrecise) {
        super.entityInside(state, level, pos, entity, effectApplier, isPrecise);
    }
    public TeleportTransition getPortalDestination(ServerLevel currentLevel, Entity entity, BlockPos portalEntryPos) {
        net.minecraft.world.level.storage.LevelData.RespawnData respawn = currentLevel.getRespawnData();
        boolean fromEnd = currentLevel.dimension() == Level.END;
        net.minecraft.resources.ResourceKey<Level> target = fromEnd ? respawn.dimension() : Level.END;
        BlockPos spawnBlock = fromEnd ? respawn.pos() : ServerLevel.END_SPAWN_POINT;
        ServerLevel newLevel = currentLevel.getServer().getLevel(target);
        if (newLevel == null) {
            return null;
        }
        Vec3 spawnPos = Vec3.atBottomCenterOf(spawnBlock);
        float yRot;
        float xRot;
        java.util.Set<net.minecraft.world.entity.Relative> relatives;
        if (!fromEnd) {
            net.minecraft.world.level.levelgen.feature.EndPlatformFeature.createEndPlatform(newLevel, BlockPos.containing(spawnPos).below(), true);
            yRot = Direction.WEST.toYRot();
            xRot = 0.0F;
            relatives = net.minecraft.world.entity.Relative.union(net.minecraft.world.entity.Relative.DELTA, java.util.Set.of(net.minecraft.world.entity.Relative.X_ROT));
            if (entity instanceof net.minecraft.server.level.ServerPlayer) {
                spawnPos = spawnPos.subtract(0.0, 1.0, 0.0);
            }
        } else {
            yRot = respawn.yaw();
            xRot = respawn.pitch();
            relatives = net.minecraft.world.entity.Relative.union(net.minecraft.world.entity.Relative.DELTA, net.minecraft.world.entity.Relative.ROTATION);
            if (entity instanceof net.minecraft.server.level.ServerPlayer player) {
                return player.findRespawnPositionAndUseSpawnBlock(false, TeleportTransition.DO_NOTHING);
            }
            spawnPos = Vec3.atBottomCenterOf(entity.adjustSpawnLocation(newLevel, spawnBlock));
        }
        return new TeleportTransition(newLevel, spawnPos, Vec3.ZERO, yRot, xRot, relatives,
            TeleportTransition.PLAY_PORTAL_SOUND.then(TeleportTransition.PLACE_PORTAL_TICKET));
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
    protected RenderShape getRenderShape(BlockState state) {
        return super.getRenderShape(state);
    }
}
