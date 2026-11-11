// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.block;
import net.minecraft.world.phys.Vec3;
import net.minecraft.core.BlockPos;
import net.minecraft.core.Direction;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.sounds.SoundEvent;
import net.minecraft.util.RandomSource;
import net.minecraft.world.entity.item.FallingBlockEntity;
import net.minecraft.world.level.Level;
import net.minecraft.world.level.LevelReader;
import net.minecraft.world.level.ScheduledTickAccess;
import net.minecraft.world.level.block.entity.BlockEntity;
import net.minecraft.world.level.block.state.BlockBehaviour;
import net.minecraft.world.level.block.state.BlockState;
import net.minecraft.world.level.block.state.StateDefinition;
public class BrushableBlock extends BaseEntityBlock implements Fallable {
    private Block turnsInto;
    private SoundEvent brushSound;
    private SoundEvent brushCompletedSound;
    public static final int TICK_DELAY = 2;
    public BrushableBlock(Block turnsInto, SoundEvent brushSound, SoundEvent brushCompletedSound, BlockBehaviour.Properties properties) {
        super((int) 0);
        this.turnsInto = turnsInto;
        this.brushSound = brushSound;
        this.brushCompletedSound = brushCompletedSound;
    }
    protected void createBlockStateDefinition(StateDefinition.Builder<Block, BlockState> builder) {
        super.createBlockStateDefinition(builder);
    }
    public void onPlace(BlockState state, Level level, BlockPos pos, BlockState oldState, boolean movedByPiston) {
        super.onPlace(state, level, pos, oldState, movedByPiston);
    }
    public BlockState updateShape(BlockState state, LevelReader level, ScheduledTickAccess ticks, BlockPos pos, Direction directionToNeighbour, BlockPos neighbourPos, BlockState neighbourState, RandomSource random) {
        return super.updateShape(state, level, ticks, pos, directionToNeighbour, neighbourPos, neighbourState, random);
    }
    public void tick(BlockState state, ServerLevel level, BlockPos pos, RandomSource random) {
        super.tick(state, level, pos, random);
    }
    public void onBrokenAfterFall(Level level, BlockPos pos, FallingBlockEntity entity) {
        Vec3 center = entity.getBoundingBox().getCenter();
        level.levelEvent(2001, BlockPos.containing(center), Block.getId(entity.getBlockState()));
        level.gameEvent(entity, net.minecraft.world.level.gameevent.GameEvent.BLOCK_DESTROY, center);
    }
    public void animateTick(BlockState state, Level level, BlockPos pos, RandomSource random) {
        super.animateTick(state, level, pos, random);
    }
    public BlockEntity newBlockEntity(BlockPos worldPosition, BlockState blockState) {
        return new net.minecraft.world.level.block.entity.BrushableBlockEntity(worldPosition, blockState);
    }
    public Block getTurnsInto() {
        return turnsInto;
    }
    public SoundEvent getBrushSound() {
        return brushSound;
    }
    public SoundEvent getBrushCompletedSound() {
        return brushCompletedSound;
    }
}
