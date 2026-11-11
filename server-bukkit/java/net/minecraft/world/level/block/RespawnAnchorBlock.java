// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.block;
import net.minecraft.util.Mth;
import net.minecraft.world.level.gameevent.GameEvent;
import net.minecraft.sounds.SoundEvents;
import net.minecraft.sounds.SoundSource;
import net.minecraft.core.Vec3i;
import net.minecraft.world.entity.vehicle.DismountHelper;
import java.util.ArrayList;
import java.util.List;
import java.util.Optional;
import net.minecraft.core.BlockPos;
import net.minecraft.core.Direction;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.util.RandomSource;
import net.minecraft.world.InteractionHand;
import net.minecraft.world.InteractionResult;
import net.minecraft.world.entity.Entity;
import net.minecraft.world.entity.EntityType;
import net.minecraft.world.entity.player.Player;
import net.minecraft.world.item.ItemStack;
import net.minecraft.world.level.CollisionGetter;
import net.minecraft.world.level.Level;
import net.minecraft.world.level.block.state.BlockBehaviour;
import net.minecraft.world.level.block.state.BlockState;
import net.minecraft.world.level.block.state.StateDefinition;
import net.minecraft.world.level.block.state.properties.IntegerProperty;
import net.minecraft.world.level.pathfinder.PathComputationType;
import net.minecraft.world.phys.BlockHitResult;
import net.minecraft.world.phys.Vec3;
public class RespawnAnchorBlock extends Block {
    private static final List<Vec3i> kStandUpOffsets = standUpOffsets();
    private static List<Vec3i> standUpOffsets() {
        int[][] ring = {{0, -1}, {-1, 0}, {0, 1}, {1, 0}, {-1, -1}, {1, -1}, {-1, 1}, {1, 1}};
        List<Vec3i> all = new ArrayList<>();
        for (int dy : new int[] {0, -1, 1}) {
            for (int[] step : ring) {
                all.add(new Vec3i(step[0], dy, step[1]));
            }
        }
        all.add(new Vec3i(0, 1, 0));
        return all;
    }
    private static Optional<Vec3> standUpAt(EntityType<?> type, CollisionGetter level, BlockPos pos, boolean checkDangerous) {
        BlockPos.MutableBlockPos cursor = new BlockPos.MutableBlockPos();
        for (Vec3i offset : kStandUpOffsets) {
            cursor.set(pos).move(offset);
            Vec3 found = DismountHelper.findSafeDismountLocation(type, level, cursor, checkDangerous);
            if (found != null) {
                return Optional.of(found);
            }
        }
        return Optional.empty();
    }
    public static final int MIN_CHARGES = 0;
    public static final int MAX_CHARGES = 4;
    public static final IntegerProperty CHARGE = net.minecraft.world.level.block.state.properties.BlockStateProperties.RESPAWN_ANCHOR_CHARGES;
    public RespawnAnchorBlock(BlockBehaviour.Properties properties) {
        super((int) 0);
    }
    protected InteractionResult useItemOn(ItemStack itemStack, BlockState state, Level level, BlockPos pos, Player player, InteractionHand hand, BlockHitResult hitResult) {
        return super.useItemOn(itemStack, state, level, pos, player, hand, hitResult);
    }
    protected InteractionResult useWithoutItem(BlockState state, Level level, BlockPos pos, Player player, BlockHitResult hitResult) {
        return super.useWithoutItem(state, level, pos, player, hitResult);
    }
    public static boolean canSetSpawn(ServerLevel level, BlockPos pos) {
        return level.environmentAttributes().getValue(net.minecraft.world.attribute.EnvironmentAttributes.RESPAWN_ANCHOR_WORKS, pos);
    }
    public static void charge(Entity sourceEntity, Level level, BlockPos pos, BlockState state) {
        BlockState charged = state.setValue(CHARGE, state.getValue(CHARGE) + 1);
        level.setBlockAndUpdate(pos, charged);
        level.gameEvent(GameEvent.BLOCK_CHANGE, pos, GameEvent.Context.of(sourceEntity, charged));
        level.playSound((Entity) null, pos.getX() + 0.5, pos.getY() + 0.5, pos.getZ() + 0.5,
            SoundEvents.RESPAWN_ANCHOR_CHARGE, SoundSource.BLOCKS, 1.0F, 1.0F);
    }
    public void animateTick(BlockState state, Level level, BlockPos pos, RandomSource random) {
        super.animateTick(state, level, pos, random);
    }
    protected void createBlockStateDefinition(StateDefinition.Builder<Block, BlockState> builder) {
        super.createBlockStateDefinition(builder);
    }
    protected boolean hasAnalogOutputSignal(BlockState state) {
        return super.hasAnalogOutputSignal(state);
    }
    public static int getScaledChargeLevel(BlockState state, int maximum) {
        return Mth.floor(state.getValue(CHARGE) / 4.0F * maximum);
    }
    protected int getAnalogOutputSignal(BlockState state, Level level, BlockPos pos, Direction direction) {
        return super.getAnalogOutputSignal(state, level, pos, direction);
    }
    public static Optional<Vec3> findStandUpPosition(EntityType<?> type, CollisionGetter level, BlockPos pos) {
        Optional<Vec3> safe = standUpAt(type, level, pos, true);
        return safe.isPresent() ? safe : standUpAt(type, level, pos, false);
    }
    protected boolean isPathfindable(BlockState state, PathComputationType type) {
        return super.isPathfindable(state, type);
    }
}
