// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.block.state;

import net.minecraft.core.BlockPos;
import net.minecraft.world.InteractionHand;
import net.minecraft.world.InteractionResult;
import net.minecraft.world.entity.player.Player;
import net.minecraft.world.item.ItemStack;
import net.minecraft.world.item.context.BlockPlaceContext;
import net.minecraft.world.level.BlockGetter;
import net.minecraft.world.level.Level;
import net.minecraft.world.level.LevelAccessor;
import net.minecraft.world.level.LevelReader;
import net.minecraft.world.level.block.Block;
import net.minecraft.world.level.material.FluidState;
import net.minecraft.world.level.material.Fluids;
import net.minecraft.world.level.material.PushReaction;
import net.minecraft.world.level.redstone.Orientation;
import net.minecraft.world.phys.BlockHitResult;
import net.minecraft.world.phys.shapes.VoxelShape;
import org.basaltmc.bukkit.Unsupported;
import org.basaltmc.bukkit.WorldNatives;
import org.basaltmc.bukkit.facade.Effects;
import org.bukkit.craftbukkit.block.data.CraftBlockData;
import com.mojang.serialization.Codec;
import net.minecraft.world.level.block.state.properties.Property;

public class BlockState extends BlockBehaviour.BlockStateBase {
    private static final int kBlocksMotion = 1;
    private static final int kOccludes = 2;
    private static final int kFullCube = 4;
    private static final int kReplaceable = 8;
    private static final int kHasCollision = 32;
    private static final int kTicksRandomly = 128;
    private static final int kAir = 256;
    private static final int kHasBlockEntity = 1 << 18;
    private static final PushReaction[] kReactions = {PushReaction.PUSH_PULL, PushReaction.POPPED,
        PushReaction.IMMOVEABLE, PushReaction.IGNORE_ENTITY, PushReaction.PUSH};
    private int flags = -1;

    public BlockState(Block block, int id) {
        super(block, id);
    }

    private int flags() {
        int found = flags;
        if (found < 0) {
            found = WorldNatives.stateFlags(stateId());
            flags = found;
        }
        return found;
    }

    public CraftBlockData asBlockData() {
        return CraftBlockData.createData(this);
    }

    public boolean isAir() {
        return (flags() & kAir) != 0;
    }

    public boolean canOcclude() {
        return (flags() & kOccludes) != 0;
    }

    public boolean isSolid() {
        return (flags() & kBlocksMotion) != 0;
    }

    public boolean canBeReplaced() {
        return (flags() & kReplaceable) != 0;
    }

    public boolean canBeReplaced(net.minecraft.world.level.material.Fluid fluid) {
        return canBeReplaced() || !isSolid();
    }

    public boolean canBeReplaced(BlockPlaceContext context) {
        return canBeReplaced();
    }

    public boolean isRandomlyTicking() {
        return (flags() & kTicksRandomly) != 0;
    }

    public boolean hasBlockEntity() {
        return (flags() & kHasBlockEntity) != 0;
    }

    public int getLightEmission() {
        return (flags() >> 9) & 15;
    }

    public int getLightDampening() {
        return (flags() >> 13) & 31;
    }

    public boolean liquid() {
        String path = getBlock().blockKey().getPath();
        return path.equals("water") || path.equals("lava");
    }

    public FluidState getFluidState() {
        if ((flags() & 16) == 0) {
            return Fluids.EMPTY.defaultFluidState();
        }
        String path = getBlock().blockKey().getPath();
        return path.equals("lava") ? Fluids.LAVA.defaultFluidState() : Fluids.WATER.defaultFluidState();
    }

    public float getDestroySpeed(BlockGetter level, BlockPos pos) {
        return (float) getBlock().facts()[0];
    }

    public boolean requiresCorrectToolForDrops() {
        return getBlock().facts()[2] != 0.0;
    }

    public PushReaction getPistonPushReaction() {
        double[] found = WorldNatives.blockInfo(stateId());
        if (found == null || found.length < 4 || found[3] < 0 || found[3] >= kReactions.length) {
            return PushReaction.PUSH_PULL;
        }
        return kReactions[(int) found[3]];
    }

    public boolean isSignalSource() {
        return false;
    }

    public boolean ignitedByLava() {
        return false;
    }

    public boolean hasAnalogOutputSignal() {
        return false;
    }

    public net.minecraft.world.level.material.MapColor getMapColor(BlockGetter level, BlockPos pos) {
        double[] found = WorldNatives.blockInfo(stateId());
        int rgb = found == null || found.length < 6 ? 0 : (int) found[5];
        for (int id = 0; id < 64; id++) {
            net.minecraft.world.level.material.MapColor candidate = net.minecraft.world.level.material.MapColor.byId(id);
            if (candidate.col == rgb) {
                return candidate;
            }
        }
        return net.minecraft.world.level.material.MapColor.NONE;
    }

    public float getShadeBrightness(BlockGetter level, BlockPos pos) {
        return isCollisionShapeFullBlock(level, pos) ? 0.2F : 1.0F;
    }

    public net.minecraft.world.level.block.RenderShape getRenderShape() {
        return isAir() ? net.minecraft.world.level.block.RenderShape.INVISIBLE
            : net.minecraft.world.level.block.RenderShape.MODEL;
    }

    public boolean emissiveRendering() {
        return false;
    }

    public long getSeed(BlockPos pos) {
        long seed = (long) (pos.getX() * 3129871) ^ (long) pos.getZ() * 116129781L ^ (long) pos.getY();
        seed = seed * seed * 42317861L + seed * 11L;
        return seed >> 16;
    }

    public net.minecraft.world.level.block.SoundType getSoundType() {
        String[] facts = WorldNatives.blockSounds(stateId());
        if (facts == null || facts.length < 7) {
            return null;
        }
        java.util.function.Function<String, net.minecraft.sounds.SoundEvent> event = id ->
            net.minecraft.sounds.SoundEvent.createVariableRangeEvent(net.minecraft.resources.Identifier.parse(id));
        return new net.minecraft.world.level.block.SoundType(Float.parseFloat(facts[0]), Float.parseFloat(facts[1]),
            event.apply(facts[2]), event.apply(facts[3]), event.apply(facts[4]), event.apply(facts[5]),
            event.apply(facts[6]));
    }

    private static String worldOf(BlockGetter level) {
        return level instanceof net.minecraft.server.level.ServerLevel server ? server.getWorld().getName() : null;
    }

    public int getSignal(BlockGetter level, BlockPos pos, net.minecraft.core.Direction direction) {
        String world = worldOf(level);
        return world == null ? 0 : org.basaltmc.bukkit.WorldEnvNatives.signal(world, pos.getX(), pos.getY(), pos.getZ(),
            direction.ordinal(), false);
    }

    public int getDirectSignal(BlockGetter level, BlockPos pos, net.minecraft.core.Direction direction) {
        String world = worldOf(level);
        return world == null ? 0 : org.basaltmc.bukkit.WorldEnvNatives.signal(world, pos.getX(), pos.getY(), pos.getZ(),
            direction.ordinal(), true);
    }

    public int getOwnSignal(BlockGetter level, BlockPos pos) {
        return getSignal(level, pos, net.minecraft.core.Direction.UP);
    }

    public BlockState rotate(net.minecraft.world.level.block.Rotation rotation) {
        int rotated = org.basaltmc.bukkit.WorldEnvNatives.transformState(stateId(), rotation.ordinal(), 0);
        BlockState found = rotated < 0 ? null : Block.stateById(rotated);
        return found == null ? this : found;
    }

    public BlockState mirror(net.minecraft.world.level.block.Mirror mirror) {
        int mirrored = org.basaltmc.bukkit.WorldEnvNatives.transformState(stateId(), 0, mirror.ordinal());
        BlockState found = mirrored < 0 ? null : Block.stateById(mirrored);
        return found == null ? this : found;
    }

    public float getDestroyProgress(Player player, BlockGetter level, BlockPos pos) {
        float hardness = getDestroySpeed(level, pos);
        if (hardness == -1.0F) {
            return 0.0F;
        }
        return player.getDestroySpeed(this) / hardness / (player.hasCorrectToolForDrops(this) ? 30 : 100);
    }

    public boolean hasLargeCollisionShapeForBlocks() {
        return hasLargeCollisionShape();
    }

    public VoxelShape shape() {
        double[] boxes = org.basaltmc.bukkit.misc.Shapes.boxes(stateId());
        if (boxes.length == 0) {
            return VoxelShape.EMPTY;
        }
        java.util.List<net.minecraft.world.phys.AABB> list = new java.util.ArrayList<>(boxes.length / 6);
        for (int i = 0; i + 5 < boxes.length; i += 6) {
            list.add(new net.minecraft.world.phys.AABB(boxes[i], boxes[i + 1], boxes[i + 2], boxes[i + 3],
                boxes[i + 4], boxes[i + 5]));
        }
        return VoxelShape.ofBoxes(list);
    }

    public VoxelShape getShape(BlockGetter level, BlockPos pos) {
        return shape();
    }

    public VoxelShape getShape(BlockGetter level, BlockPos pos,
            net.minecraft.world.phys.shapes.CollisionContext context) {
        return shape();
    }

    public VoxelShape getCollisionShape(BlockGetter level, BlockPos pos) {
        return (flags() & kHasCollision) == 0 ? VoxelShape.EMPTY : shape();
    }

    public VoxelShape getCollisionShape(BlockGetter level, BlockPos pos,
            net.minecraft.world.phys.shapes.CollisionContext context) {
        return getCollisionShape(level, pos);
    }

    public VoxelShape getVisualShape(BlockGetter level, BlockPos pos,
            net.minecraft.world.phys.shapes.CollisionContext context) {
        return shape();
    }

    public VoxelShape getInteractionShape(BlockGetter level, BlockPos pos) {
        return VoxelShape.EMPTY;
    }

    public VoxelShape getBlockSupportShape(BlockGetter level, BlockPos pos) {
        return getCollisionShape(level, pos);
    }

    public VoxelShape getOcclusionShape() {
        return canOcclude() ? shape() : VoxelShape.EMPTY;
    }

    public boolean isCollisionShapeFullBlock(BlockGetter level, BlockPos pos) {
        return getCollisionShape(level, pos).isFullBlock();
    }

    public boolean isSolidRender() {
        return (flags() & (kOccludes | kFullCube)) == (kOccludes | kFullCube);
    }

    public boolean isFaceSturdy(BlockGetter level, BlockPos pos, net.minecraft.core.Direction direction) {
        return isFaceSturdy(level, pos, direction, net.minecraft.world.level.block.SupportType.FULL);
    }

    public boolean isFaceSturdy(BlockGetter level, BlockPos pos, net.minecraft.core.Direction direction,
            net.minecraft.world.level.block.SupportType supportType) {
        double[] boxes = org.basaltmc.bukkit.misc.Shapes.boxes(stateId());
        net.minecraft.core.Direction.Axis axis = direction.getAxis();
        boolean positive = direction.getAxisDirection() == net.minecraft.core.Direction.AxisDirection.POSITIVE;
        double lo = supportType == net.minecraft.world.level.block.SupportType.CENTER ? 0.375 : 0.0;
        double hi = supportType == net.minecraft.world.level.block.SupportType.CENTER ? 0.625 : 1.0;
        int a = axis.ordinal();
        int u = (a + 1) % 3;
        int v = (a + 2) % 3;
        for (int i = 0; i + 5 < boxes.length; i += 6) {
            double faceCoord = positive ? boxes[i + 3 + a] : boxes[i + a];
            if (faceCoord != (positive ? 1.0 : 0.0)) {
                continue;
            }
            if (boxes[i + u] <= lo && boxes[i + 3 + u] >= hi && boxes[i + v] <= lo && boxes[i + 3 + v] >= hi) {
                return true;
            }
        }
        return false;
    }

    public boolean useShapeForLightOcclusion() {
        return false;
    }

    public boolean hasLargeCollisionShape() {
        return !shape().isEmpty() && !shape().isFullBlock() && shape().bounds().getXsize() > 1.0;
    }

    public boolean isRedstoneConductor(BlockGetter level, BlockPos pos) {
        return isSolidRender();
    }

    public BlockPos getPostProcessPos(BlockGetter level, BlockPos pos) {
        return pos;
    }

    public boolean canSurvive(LevelReader level, BlockPos pos) {
        return getBlock().canSurvive(this, level, pos);
    }

    public void onPlace(Level level, BlockPos pos, BlockState oldState, boolean movedByPiston) {
        if (getBlock().blockIndex() < 0) {
            getBlock().onPlace(this, level, pos, oldState, movedByPiston);
        } else {
            Effects.placed(level, pos, oldState);
        }
    }

    public void handleNeighborChanged(Level level, BlockPos pos, Block neighbor, Orientation orientation, boolean movedByPiston) {
        if (getBlock().blockIndex() < 0) {
            getBlock().neighborChanged(this, level, pos, neighbor, orientation, movedByPiston);
        } else {
            Effects.neighborChanged(level, pos, neighbor == null ? null : neighbor.defaultBlockState());
        }
    }

    public boolean triggerEvent(Level level, BlockPos pos, int id, int param) {
        return getBlock().triggerEvent(this, level, pos, id, param);
    }

    public void updateNeighbourShapes(LevelAccessor level, BlockPos pos, int flags, int recursionLeft) {
        Effects.shapes(level, pos);
    }

    public void updateIndirectNeighbourShapes(LevelAccessor level, BlockPos pos, int flags, int recursionLeft) {
        Effects.indirectShapes(level, pos, this);
    }

    public InteractionResult useItemOn(ItemStack stack, Level level, Player player, InteractionHand hand,
            BlockHitResult hit) {
        return getBlock().useItemOn(stack, this, level, hit.getBlockPos(), player, hand, hit);
    }

    @Override
    public String toString() {
        return "Block{" + getBlock().blockKey() + "}" + WorldNatives.stateText(stateId()).replaceFirst("^[^\\[]*", "");
    }
    public static final Codec<BlockState> FULL_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.codec(net.minecraft.world.level.block.state.BlockState.class, "FULL_CODEC");
    public static final Codec<BlockState> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.codec(net.minecraft.world.level.block.state.BlockState.class, "CODEC");
    public BlockState(Block owner, Property<?>[] propertyKeys, Comparable<?>[] propertyValues) {
        super((net.minecraft.world.level.block.Block) null, (net.minecraft.world.level.block.state.properties.Property[]) null, (Comparable[]) null); throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.block.state.BlockState#<init>");
    }
    protected BlockState asState() {
        return this;
    }

    @Override
    public net.minecraft.core.Holder<net.minecraft.world.level.block.Block> typeHolder() {
        return net.minecraft.core.registries.BuiltInRegistries.BLOCK.wrapAsHolder(getBlock());
    }

    @Override
    public boolean isValidSpawn(net.minecraft.world.level.BlockGetter level, net.minecraft.core.BlockPos pos,
            net.minecraft.world.entity.EntityType<?> type) {
        return isFaceSturdy(level, pos, net.minecraft.core.Direction.UP) && getLightEmission() < 14;
    }

    @Override
    public boolean propagatesSkylightDown() {
        return !isSolidRender() && getFluidState().isEmpty();
    }

    @Override
    public boolean shouldRedstoneWireConnectTo(net.minecraft.world.level.BlockGetter level, net.minecraft.core.BlockPos pos,
            net.minecraft.core.Direction direction) {
        return isSignalSource();
    }

    @Override
    public int getAnalogOutputSignal(net.minecraft.world.level.Level level, net.minecraft.core.BlockPos pos,
            net.minecraft.core.Direction direction) {
        return 0;
    }

    @Override
    public boolean skipRendering(BlockState neighborState, net.minecraft.core.Direction direction) {
        return false;
    }

    @Override
    public boolean hasOffsetFunction() {
        return false;
    }

    @Override
    public net.minecraft.world.phys.Vec3 getOffset(net.minecraft.core.BlockPos pos) {
        return net.minecraft.world.phys.Vec3.ZERO;
    }

    @Override
    public void affectNeighborsAfterRemoval(net.minecraft.server.level.ServerLevel level, net.minecraft.core.BlockPos pos,
            boolean movedByPiston) {
        getBlock().affectNeighborsAfterRemoval(this, level, pos, movedByPiston);
    }

    @Override
    public void tick(net.minecraft.server.level.ServerLevel level, net.minecraft.core.BlockPos pos,
            net.minecraft.util.RandomSource random) {
        getBlock().tick(this, level, pos, random);
    }

    @Override
    public void randomTick(net.minecraft.server.level.ServerLevel level, net.minecraft.core.BlockPos pos,
            net.minecraft.util.RandomSource random) {
        getBlock().randomTick(this, level, pos, random);
    }

    @Override
    public void spawnAfterBreak(net.minecraft.server.level.ServerLevel level, net.minecraft.core.BlockPos pos,
            net.minecraft.world.item.ItemStack tool, boolean dropExperience) {
        getBlock().spawnAfterBreak(this, level, pos, tool, dropExperience);
    }

    @Override
    public void attack(net.minecraft.world.level.Level level, net.minecraft.core.BlockPos pos,
            net.minecraft.world.entity.player.Player player) {
        getBlock().attack(this, level, pos, player);
    }

    @Override
    public void entityInside(net.minecraft.world.level.Level level, net.minecraft.core.BlockPos pos,
            net.minecraft.world.entity.Entity entity, net.minecraft.world.entity.InsideBlockEffectApplier effectApplier,
            boolean isPrecise) {
        getBlock().entityInside(this, level, pos, entity, effectApplier, isPrecise);
    }

    @Override
    public void onExplosionHit(net.minecraft.server.level.ServerLevel level, net.minecraft.core.BlockPos pos,
            net.minecraft.world.level.Explosion explosion,
            java.util.function.BiConsumer<net.minecraft.world.item.ItemStack, net.minecraft.core.BlockPos> onHit) {
        getBlock().onExplosionHit(this, level, pos, explosion, onHit);
    }

    @Override
    public net.minecraft.world.InteractionResult useWithoutItem(net.minecraft.world.level.Level level,
            net.minecraft.world.entity.player.Player player, net.minecraft.world.phys.BlockHitResult hitResult) {
        return getBlock().useWithoutItem(this, level, hitResult.getBlockPos(), player, hitResult);
    }

    @Override
    public boolean showAsInteractableInSpectatorMode(net.minecraft.world.level.Level level, net.minecraft.core.BlockPos pos,
            net.minecraft.world.phys.BlockHitResult hitResult) {
        return getBlock().showAsInteractableInSpectatorMode(this, level, pos, hitResult);
    }

    @Override
    public boolean isSuffocating(net.minecraft.world.level.BlockGetter level, net.minecraft.core.BlockPos pos) {
        return isSolid() && isCollisionShapeFullBlock(level, pos);
    }

    @Override
    public boolean isViewBlocking(net.minecraft.world.level.BlockGetter level, net.minecraft.core.BlockPos blockPos,
            net.minecraft.world.phys.AABB nearPlaneBox) {
        return isSuffocating(level, blockPos);
    }

    @Override
    public boolean isLightPermeable() {
        return !isSolidRender();
    }

    @Override
    public BlockState updateShape(net.minecraft.world.level.LevelReader level, net.minecraft.world.level.ScheduledTickAccess ticks,
            net.minecraft.core.BlockPos pos, net.minecraft.core.Direction directionToNeighbour, net.minecraft.core.BlockPos neighbourPos,
            BlockState neighbourState, net.minecraft.util.RandomSource random) {
        return getBlock().updateShape(this, level, ticks, pos, directionToNeighbour, neighbourPos, neighbourState, random);
    }

    @Override
    public net.minecraft.world.MenuProvider getMenuProvider(net.minecraft.world.level.Level level, net.minecraft.core.BlockPos pos) {
        return getBlock().getMenuProvider(this, level, pos);
    }

    @Override
    public boolean shouldChangedStateKeepBlockEntity(BlockState oldState) {
        return oldState.getBlock() == getBlock();
    }

    @Override
    public void onProjectileHit(net.minecraft.world.level.Level level, BlockState state, net.minecraft.world.phys.BlockHitResult blockHit,
            net.minecraft.world.entity.projectile.Projectile entity) {
    }

    @Override
    public net.minecraft.world.item.ItemStack getCloneItemStack(net.minecraft.world.level.LevelReader level, net.minecraft.core.BlockPos pos,
            boolean includeData) {
        return new net.minecraft.world.item.ItemStack(getBlock().asItem());
    }

    @Override
    public boolean shouldSpawnTerrainParticles() {
        return true;
    }

    @Override
    public BlockState withPropertiesOf(BlockState source) {
        BlockState result = this;
        for (Property<?> property : getProperties()) {
            if (source.hasProperty(property)) {
                result = copyProperty(source, result, property);
            }
        }
        return result;
    }

    public static <T extends Comparable<T>> BlockState copyProperty(BlockState from, BlockState to, Property<T> property) {
        return to.setValue(property, from.getValue(property));
    }
}
