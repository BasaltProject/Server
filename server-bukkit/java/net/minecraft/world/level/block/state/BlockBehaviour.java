// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.block.state;

import java.util.Collection;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;
import net.minecraft.core.TypedInstance;
import net.minecraft.world.flag.FeatureElement;
import net.minecraft.world.level.block.Block;
import net.minecraft.world.level.block.state.properties.Property;
import java.util.Optional;
import java.util.function.BiConsumer;
import java.util.function.Function;
import java.util.function.Predicate;
import java.util.function.ToIntFunction;
import net.minecraft.core.BlockPos;
import net.minecraft.core.Direction;
import net.minecraft.core.Holder;
import net.minecraft.resources.ResourceKey;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.tags.TagKey;
import net.minecraft.util.RandomSource;
import net.minecraft.world.InteractionHand;
import net.minecraft.world.InteractionResult;
import net.minecraft.world.MenuProvider;
import net.minecraft.world.entity.Entity;
import net.minecraft.world.entity.EntityType;
import net.minecraft.world.entity.InsideBlockEffectApplier;
import net.minecraft.world.entity.player.Player;
import net.minecraft.world.entity.projectile.Projectile;
import net.minecraft.world.flag.FeatureFlag;
import net.minecraft.world.flag.FeatureFlagSet;
import net.minecraft.world.item.DyeColor;
import net.minecraft.world.item.Item;
import net.minecraft.world.item.ItemStack;
import net.minecraft.world.item.context.BlockPlaceContext;
import net.minecraft.world.level.BlockGetter;
import net.minecraft.world.level.Explosion;
import net.minecraft.world.level.Level;
import net.minecraft.world.level.LevelAccessor;
import net.minecraft.world.level.LevelReader;
import net.minecraft.world.level.ScheduledTickAccess;
import net.minecraft.world.level.block.Mirror;
import net.minecraft.world.level.block.RenderShape;
import net.minecraft.world.level.block.Rotation;
import net.minecraft.world.level.block.SoundType;
import net.minecraft.world.level.block.SupportType;
import net.minecraft.world.level.block.entity.BlockEntity;
import net.minecraft.world.level.block.entity.BlockEntityTicker;
import net.minecraft.world.level.block.entity.BlockEntityType;
import net.minecraft.world.level.block.state.properties.NoteBlockInstrument;
import net.minecraft.world.level.material.Fluid;
import net.minecraft.world.level.material.FluidState;
import net.minecraft.world.level.material.MapColor;
import net.minecraft.world.level.material.PushReaction;
import net.minecraft.world.level.pathfinder.PathComputationType;
import net.minecraft.world.level.redstone.Orientation;
import net.minecraft.world.level.storage.loot.LootParams;
import net.minecraft.world.level.storage.loot.LootTable;
import net.minecraft.world.phys.AABB;
import net.minecraft.world.phys.BlockHitResult;
import net.minecraft.world.phys.Vec3;
import net.minecraft.world.phys.shapes.CollisionContext;
import net.minecraft.world.phys.shapes.VoxelShape;

public abstract class BlockBehaviour implements FeatureElement {
    public abstract static class BlockStateBase extends StateHolder<Block, BlockState> implements TypedInstance<Block> {
        private final int id;

        protected BlockStateBase(Block block, int id) {
            super(block);
            this.id = id;
        }

        public int stateId() {
            return id;
        }

        public Block getBlock() {
            return owner;
        }

        @Override
        public Collection<Property<?>> getProperties() {
            return List.of(owner.propertiesInOrder());
        }

        @Override
        public <T extends Comparable<T>> boolean hasProperty(Property<T> property) {
            return owner.indexOf(property) >= 0;
        }

        @Override
        public <T extends Comparable<T>> T getValue(Property<T> property) {
            int index = owner.indexOf(property);
            if (index < 0) {
                throw new IllegalArgumentException("Cannot get property " + property + " as it does not exist in " + owner);
            }
            return property.getPossibleValues().get(valueIndex(index));
        }

        @Override
        public <T extends Comparable<T>, V extends T> BlockState setValue(Property<T> property, V value) {
            int index = owner.indexOf(property);
            if (index < 0) {
                throw new IllegalArgumentException("Cannot set property " + property + " as it does not exist in " + owner);
            }
            int wanted = property.getPossibleValues().indexOf(value);
            if (wanted < 0) {
                throw new IllegalArgumentException("Cannot set property " + property + " to " + value + " on " + owner);
            }
            int offset = id - owner.firstState();
            int stride = owner.strideOf(index);
            return owner.stateAtOffset(offset + (wanted - valueIndex(index)) * stride);
        }

        @Override
        public Map<Property<?>, Comparable<?>> getValues() {
            Map<Property<?>, Comparable<?>> values = new LinkedHashMap<>();
            Property<?>[] properties = owner.propertiesInOrder();
            for (int i = 0; i < properties.length; i++) {
                values.put(properties[i], properties[i].getPossibleValues().get(valueIndex(i)));
            }
            return values;
        }

        private int valueIndex(int index) {
            Property<?> property = owner.propertiesInOrder()[index];
            return ((id - owner.firstState()) / owner.strideOf(index)) % property.getPossibleValues().size();
        }
            protected BlockStateBase(Block owner, Property<?>[] propertyKeys, Comparable<?>[] propertyValues) {
            super(org.basaltmc.bukkit.facade.NmsStub.nul(), (net.minecraft.world.level.block.state.properties.Property[]) null, (Comparable[]) null); throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.block.state.BlockBehaviour.BlockStateBase#<init>");
        }
        public void initCache() {
            throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.block.state.BlockBehaviour.BlockStateBase#initCache");
        }
        public Holder<Block> typeHolder() {
            throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.block.state.BlockBehaviour.BlockStateBase#typeHolder");
        }
        public boolean isSolid() {
            throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.block.state.BlockBehaviour.BlockStateBase#isSolid");
        }
        public boolean isValidSpawn(BlockGetter level, BlockPos pos, EntityType<?> type) {
            throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.block.state.BlockBehaviour.BlockStateBase#isValidSpawn");
        }
        public boolean propagatesSkylightDown() {
            throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.block.state.BlockBehaviour.BlockStateBase#propagatesSkylightDown");
        }
        public int getLightDampening() {
            throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.block.state.BlockBehaviour.BlockStateBase#getLightDampening");
        }
        public VoxelShape getFaceOcclusionShape(Direction direction) {
            throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.block.state.BlockBehaviour.BlockStateBase#getFaceOcclusionShape");
        }
        public VoxelShape getOcclusionShape() {
            throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.block.state.BlockBehaviour.BlockStateBase#getOcclusionShape");
        }
        public boolean hasLargeCollisionShape() {
            throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.block.state.BlockBehaviour.BlockStateBase#hasLargeCollisionShape");
        }
        public boolean useShapeForLightOcclusion() {
            throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.block.state.BlockBehaviour.BlockStateBase#useShapeForLightOcclusion");
        }
        public int getLightEmission() {
            throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.block.state.BlockBehaviour.BlockStateBase#getLightEmission");
        }
        public boolean isAir() {
            throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.block.state.BlockBehaviour.BlockStateBase#isAir");
        }
        public boolean ignitedByLava() {
            throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.block.state.BlockBehaviour.BlockStateBase#ignitedByLava");
        }
        public boolean liquid() {
            throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.block.state.BlockBehaviour.BlockStateBase#liquid");
        }
        public MapColor getMapColor(BlockGetter level, BlockPos pos) {
            throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.block.state.BlockBehaviour.BlockStateBase#getMapColor");
        }
        public BlockState rotate(Rotation rotation) {
            throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.block.state.BlockBehaviour.BlockStateBase#rotate");
        }
        public BlockState mirror(Mirror mirror) {
            throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.block.state.BlockBehaviour.BlockStateBase#mirror");
        }
        public RenderShape getRenderShape() {
            throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.block.state.BlockBehaviour.BlockStateBase#getRenderShape");
        }
        public boolean emissiveRendering() {
            throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.block.state.BlockBehaviour.BlockStateBase#emissiveRendering");
        }
        public float getShadeBrightness(BlockGetter level, BlockPos pos) {
            throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.block.state.BlockBehaviour.BlockStateBase#getShadeBrightness");
        }
        public boolean isRedstoneConductor(BlockGetter level, BlockPos pos) {
            throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.block.state.BlockBehaviour.BlockStateBase#isRedstoneConductor");
        }
        public boolean isSignalSource() {
            throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.block.state.BlockBehaviour.BlockStateBase#isSignalSource");
        }
        public boolean shouldRedstoneWireConnectTo(BlockGetter level, BlockPos pos, Direction direction) {
            throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.block.state.BlockBehaviour.BlockStateBase#shouldRedstoneWireConnectTo");
        }
        public int getOwnSignal(BlockGetter level, BlockPos pos) {
            throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.block.state.BlockBehaviour.BlockStateBase#getOwnSignal");
        }
        public int getSignal(BlockGetter level, BlockPos pos, Direction direction) {
            throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.block.state.BlockBehaviour.BlockStateBase#getSignal");
        }
        public boolean hasAnalogOutputSignal() {
            throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.block.state.BlockBehaviour.BlockStateBase#hasAnalogOutputSignal");
        }
        public int getAnalogOutputSignal(Level level, BlockPos pos, Direction direction) {
            throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.block.state.BlockBehaviour.BlockStateBase#getAnalogOutputSignal");
        }
        public float getDestroySpeed(BlockGetter level, BlockPos pos) {
            throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.block.state.BlockBehaviour.BlockStateBase#getDestroySpeed");
        }
        public float getDestroyProgress(Player player, BlockGetter level, BlockPos pos) {
            throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.block.state.BlockBehaviour.BlockStateBase#getDestroyProgress");
        }
        public int getDirectSignal(BlockGetter level, BlockPos pos, Direction direction) {
            throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.block.state.BlockBehaviour.BlockStateBase#getDirectSignal");
        }
        public PushReaction getPistonPushReaction() {
            throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.block.state.BlockBehaviour.BlockStateBase#getPistonPushReaction");
        }
        public boolean isSolidRender() {
            throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.block.state.BlockBehaviour.BlockStateBase#isSolidRender");
        }
        public boolean canOcclude() {
            throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.block.state.BlockBehaviour.BlockStateBase#canOcclude");
        }
        public boolean skipRendering(BlockState neighborState, Direction direction) {
            throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.block.state.BlockBehaviour.BlockStateBase#skipRendering");
        }
        public VoxelShape getShape(BlockGetter level, BlockPos pos) {
            throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.block.state.BlockBehaviour.BlockStateBase#getShape");
        }
        public VoxelShape getShape(BlockGetter level, BlockPos pos, CollisionContext context) {
            throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.block.state.BlockBehaviour.BlockStateBase#getShape");
        }
        public VoxelShape getCollisionShape(BlockGetter level, BlockPos pos) {
            throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.block.state.BlockBehaviour.BlockStateBase#getCollisionShape");
        }
        public VoxelShape getCollisionShape(BlockGetter level, BlockPos pos, CollisionContext context) {
            throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.block.state.BlockBehaviour.BlockStateBase#getCollisionShape");
        }
        public VoxelShape getEntityInsideCollisionShape(BlockGetter level, BlockPos pos, Entity entity) {
            throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.block.state.BlockBehaviour.BlockStateBase#getEntityInsideCollisionShape");
        }
        public VoxelShape getBlockSupportShape(BlockGetter level, BlockPos pos) {
            throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.block.state.BlockBehaviour.BlockStateBase#getBlockSupportShape");
        }
        public VoxelShape getVisualShape(BlockGetter level, BlockPos pos, CollisionContext context) {
            throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.block.state.BlockBehaviour.BlockStateBase#getVisualShape");
        }
        public VoxelShape getInteractionShape(BlockGetter level, BlockPos pos) {
            throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.block.state.BlockBehaviour.BlockStateBase#getInteractionShape");
        }
        public boolean entityCanStandOn(BlockGetter level, BlockPos pos, Entity entity) {
            throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.block.state.BlockBehaviour.BlockStateBase#entityCanStandOn");
        }
        public boolean entityCanStandOnFace(BlockGetter level, BlockPos pos, Entity entity, Direction faceDirection) {
            throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.block.state.BlockBehaviour.BlockStateBase#entityCanStandOnFace");
        }
        public Vec3 getOffset(BlockPos pos) {
            throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.block.state.BlockBehaviour.BlockStateBase#getOffset");
        }
        public boolean hasOffsetFunction() {
            throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.block.state.BlockBehaviour.BlockStateBase#hasOffsetFunction");
        }
        public boolean triggerEvent(Level level, BlockPos pos, int b0, int b1) {
            throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.block.state.BlockBehaviour.BlockStateBase#triggerEvent");
        }
        public void handleNeighborChanged(Level level, BlockPos pos, Block block, Orientation orientation, boolean movedByPiston) {
            throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.block.state.BlockBehaviour.BlockStateBase#handleNeighborChanged");
        }
        public void updateNeighbourShapes(LevelAccessor level, BlockPos pos, int updateFlags) {
            throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.block.state.BlockBehaviour.BlockStateBase#updateNeighbourShapes");
        }
        public void updateNeighbourShapes(LevelAccessor level, BlockPos pos, int updateFlags, int updateLimit) {
            throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.block.state.BlockBehaviour.BlockStateBase#updateNeighbourShapes");
        }
        public void updateIndirectNeighbourShapes(LevelAccessor level, BlockPos pos, int updateFlags) {
            throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.block.state.BlockBehaviour.BlockStateBase#updateIndirectNeighbourShapes");
        }
        public void updateIndirectNeighbourShapes(LevelAccessor level, BlockPos pos, int updateFlags, int updateLimit) {
            throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.block.state.BlockBehaviour.BlockStateBase#updateIndirectNeighbourShapes");
        }
        public void onPlace(Level level, BlockPos pos, BlockState oldState, boolean movedByPiston) {
            throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.block.state.BlockBehaviour.BlockStateBase#onPlace");
        }
        public void affectNeighborsAfterRemoval(ServerLevel level, BlockPos pos, boolean movedByPiston) {
            throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.block.state.BlockBehaviour.BlockStateBase#affectNeighborsAfterRemoval");
        }
        public void onExplosionHit(ServerLevel level, BlockPos pos, Explosion explosion, BiConsumer<ItemStack, BlockPos> onHit) {
            throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.block.state.BlockBehaviour.BlockStateBase#onExplosionHit");
        }
        public void tick(ServerLevel level, BlockPos pos, RandomSource random) {
            throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.block.state.BlockBehaviour.BlockStateBase#tick");
        }
        public void randomTick(ServerLevel level, BlockPos pos, RandomSource random) {
            throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.block.state.BlockBehaviour.BlockStateBase#randomTick");
        }
        public void entityInside(Level level, BlockPos pos, Entity entity, InsideBlockEffectApplier effectApplier, boolean isPrecise) {
            throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.block.state.BlockBehaviour.BlockStateBase#entityInside");
        }
        public void spawnAfterBreak(ServerLevel level, BlockPos pos, ItemStack tool, boolean dropExperience) {
            throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.block.state.BlockBehaviour.BlockStateBase#spawnAfterBreak");
        }
        public List<ItemStack> getDrops(LootParams.Builder params) {
            throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.block.state.BlockBehaviour.BlockStateBase#getDrops");
        }
        public InteractionResult useItemOn(ItemStack itemStack, Level level, Player player, InteractionHand hand, BlockHitResult hitResult) {
            throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.block.state.BlockBehaviour.BlockStateBase#useItemOn");
        }
        public InteractionResult useWithoutItem(Level level, Player player, BlockHitResult hitResult) {
            throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.block.state.BlockBehaviour.BlockStateBase#useWithoutItem");
        }
        public boolean showAsInteractableInSpectatorMode(Level level, BlockPos pos, BlockHitResult hitResult) {
            throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.block.state.BlockBehaviour.BlockStateBase#showAsInteractableInSpectatorMode");
        }
        public void attack(Level level, BlockPos pos, Player player) {
            throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.block.state.BlockBehaviour.BlockStateBase#attack");
        }
        public boolean isSuffocating(BlockGetter level, BlockPos pos) {
            throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.block.state.BlockBehaviour.BlockStateBase#isSuffocating");
        }
        public boolean isLightPermeable() {
            throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.block.state.BlockBehaviour.BlockStateBase#isLightPermeable");
        }
        public boolean isViewBlocking(BlockGetter level, BlockPos blockPos, AABB nearPlaneBox) {
            throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.block.state.BlockBehaviour.BlockStateBase#isViewBlocking");
        }
        public BlockState updateShape(LevelReader level, ScheduledTickAccess ticks, BlockPos pos, Direction directionToNeighbour, BlockPos neighbourPos, BlockState neighbourState, RandomSource random) {
            throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.block.state.BlockBehaviour.BlockStateBase#updateShape");
        }
        public boolean isPathfindable(PathComputationType type) {
            throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.block.state.BlockBehaviour.BlockStateBase#isPathfindable");
        }
        public boolean canBeReplaced(BlockPlaceContext context) {
            throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.block.state.BlockBehaviour.BlockStateBase#canBeReplaced");
        }
        public boolean canBeReplaced(Fluid fluid) {
            throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.block.state.BlockBehaviour.BlockStateBase#canBeReplaced");
        }
        public boolean canBeReplaced() {
            throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.block.state.BlockBehaviour.BlockStateBase#canBeReplaced");
        }
        public boolean canSurvive(LevelReader level, BlockPos pos) {
            throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.block.state.BlockBehaviour.BlockStateBase#canSurvive");
        }
        public BlockPos getPostProcessPos(BlockGetter level, BlockPos pos) {
            throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.block.state.BlockBehaviour.BlockStateBase#getPostProcessPos");
        }
        public MenuProvider getMenuProvider(Level level, BlockPos pos) {
            throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.block.state.BlockBehaviour.BlockStateBase#getMenuProvider");
        }
        public boolean is(TagKey<Block> tag, Predicate<BlockStateBase> predicate) {
            throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.block.state.BlockBehaviour.BlockStateBase#is");
        }
        public boolean hasBlockEntity() {
            throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.block.state.BlockBehaviour.BlockStateBase#hasBlockEntity");
        }
        public boolean shouldChangedStateKeepBlockEntity(BlockState oldState) {
            throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.block.state.BlockBehaviour.BlockStateBase#shouldChangedStateKeepBlockEntity");
        }
        public <T extends BlockEntity> BlockEntityTicker<T> getTicker(Level level, BlockEntityType<T> type) {
            throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.block.state.BlockBehaviour.BlockStateBase#getTicker");
        }
        public FluidState getFluidState() {
            throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.block.state.BlockBehaviour.BlockStateBase#getFluidState");
        }
        public boolean isRandomlyTicking() {
            throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.block.state.BlockBehaviour.BlockStateBase#isRandomlyTicking");
        }
        public long getSeed(BlockPos pos) {
            throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.block.state.BlockBehaviour.BlockStateBase#getSeed");
        }
        public SoundType getSoundType() {
            throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.block.state.BlockBehaviour.BlockStateBase#getSoundType");
        }
        public void onProjectileHit(Level level, BlockState state, BlockHitResult blockHit, Projectile entity) {
            throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.block.state.BlockBehaviour.BlockStateBase#onProjectileHit");
        }
        public boolean isFaceSturdy(BlockGetter level, BlockPos pos, Direction direction) {
            throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.block.state.BlockBehaviour.BlockStateBase#isFaceSturdy");
        }
        public boolean isFaceSturdy(BlockGetter level, BlockPos pos, Direction direction, SupportType supportType) {
            throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.block.state.BlockBehaviour.BlockStateBase#isFaceSturdy");
        }
        public boolean isCollisionShapeFullBlock(BlockGetter level, BlockPos pos) {
            throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.block.state.BlockBehaviour.BlockStateBase#isCollisionShapeFullBlock");
        }
        public ItemStack getCloneItemStack(LevelReader level, BlockPos pos, boolean includeData) {
            throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.block.state.BlockBehaviour.BlockStateBase#getCloneItemStack");
        }
        protected BlockState asState() {
            throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.block.state.BlockBehaviour.BlockStateBase#asState");
        }
        public boolean requiresCorrectToolForDrops() {
            throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.block.state.BlockBehaviour.BlockStateBase#requiresCorrectToolForDrops");
        }
        public boolean shouldSpawnTerrainParticles() {
            throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.block.state.BlockBehaviour.BlockStateBase#shouldSpawnTerrainParticles");
        }
        public BlockState withPropertiesOf(BlockState source) {
            throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.block.state.BlockBehaviour.BlockStateBase#withPropertiesOf");
        }
        public static <T extends Comparable<T>> BlockState copyProperty(BlockState from, BlockState to, Property<T> property) {
            throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.block.state.BlockBehaviour.BlockStateBase#copyProperty");
        }
        public NoteBlockInstrument instrument() {
            throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.block.state.BlockBehaviour.BlockStateBase#instrument");
        }
        static final class Cache {
            public final VoxelShape collisionShape = null;
            public final boolean largeCollisionShape = false;
            public final boolean isCollisionShapeFullBlock = false;
            private Cache(BlockState state) {
                throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.block.state.BlockBehaviour.BlockStateBase.Cache#<init>");
            }
            public boolean isFaceSturdy(Direction direction, SupportType supportType) {
                throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.block.state.BlockBehaviour.BlockStateBase.Cache#isFaceSturdy");
            }
        }
}
    protected static final Direction[] UPDATE_SHAPE_ORDER = new Direction[] {Direction.WEST, Direction.EAST, Direction.NORTH, Direction.SOUTH, Direction.DOWN, Direction.UP};
    protected boolean hasCollision;
    protected float explosionResistance;
    protected boolean isRandomlyTicking;
    protected SoundType soundType;
    protected float friction;
    protected float speedFactor;
    protected float jumpFactor;
    protected float bounceRestitution;
    protected float fallDistanceReduction;
    protected boolean dynamicShape;
    protected FeatureFlagSet requiredFeatures;
    protected Properties properties;
    protected Optional<ResourceKey<LootTable>> drops;
    protected String descriptionId;
    public BlockBehaviour(Properties properties) {
        this.properties = properties;
        this.hasCollision = properties.hasCollision;
        this.explosionResistance = properties.explosionResistance;
        this.isRandomlyTicking = properties.isRandomlyTicking;
        this.soundType = properties.soundType;
        this.friction = properties.friction;
        this.speedFactor = properties.speedFactor;
        this.jumpFactor = properties.jumpFactor;
        this.bounceRestitution = properties.bounceRestitution;
        this.fallDistanceReduction = properties.fallDistanceReduction;
        this.dynamicShape = properties.dynamicShape;
        this.requiredFeatures = properties.requiredFeatures;
        this.drops = properties.effectiveDrops();
        this.descriptionId = properties.effectiveDescriptionId();
    }
    public Properties properties() {
        return properties;
    }
    protected void updateIndirectNeighbourShapes(BlockState state, LevelAccessor level, BlockPos pos, int updateFlags, int updateLimit) {

    }
    protected boolean isPathfindable(BlockState state, PathComputationType type) {
        return switch (type) {
            case LAND, AIR -> !state.isSolid();
            case WATER -> state.getFluidState().getType() == net.minecraft.world.level.material.Fluids.WATER;
        };
    }
    protected BlockState updateShape(BlockState state, LevelReader level, ScheduledTickAccess ticks, BlockPos pos, Direction directionToNeighbour, BlockPos neighbourPos, BlockState neighbourState, RandomSource random) {
        return state;
    }
    protected boolean skipRendering(BlockState state, BlockState neighborState, Direction direction) {
        return false;
    }
    protected void neighborChanged(BlockState state, Level level, BlockPos pos, Block block, Orientation orientation, boolean movedByPiston) {

    }
    protected void onPlace(BlockState state, Level level, BlockPos pos, BlockState oldState, boolean movedByPiston) {

    }
    protected void affectNeighborsAfterRemoval(BlockState state, ServerLevel level, BlockPos pos, boolean movedByPiston) {

    }
    protected void onExplosionHit(BlockState state, ServerLevel level, BlockPos pos, Explosion explosion, BiConsumer<ItemStack, BlockPos> onHit) {

    }
    protected InteractionResult useWithoutItem(BlockState state, Level level, BlockPos pos, Player player, BlockHitResult hitResult) {
        return InteractionResult.PASS;
    }
    protected InteractionResult useItemOn(ItemStack itemStack, BlockState state, Level level, BlockPos pos, Player player, InteractionHand hand, BlockHitResult hitResult) {
        return InteractionResult.TRY_WITH_EMPTY_HAND;
    }
    protected boolean showAsInteractableInSpectatorMode(BlockState state, Level level, BlockPos pos, BlockHitResult hitResult) {
        return false;
    }
    protected boolean triggerEvent(BlockState state, Level level, BlockPos pos, int b0, int b1) {
        return false;
    }
    protected RenderShape getRenderShape(BlockState state) {
        return RenderShape.MODEL;
    }
    protected boolean useShapeForLightOcclusion(BlockState state) {
        return false;
    }
    protected FluidState getFluidState(BlockState state) {
        return net.minecraft.world.level.material.Fluids.EMPTY.defaultFluidState();
    }
    protected boolean hasAnalogOutputSignal(BlockState state) {
        return false;
    }
    protected float getMaxHorizontalOffset() {
        return 0.25F;
    }
    protected float getMaxVerticalOffset() {
        return 0.2F;
    }
    public FeatureFlagSet requiredFeatures() {
        return FeatureFlagSet.of();
    }
    protected boolean shouldChangedStateKeepBlockEntity(BlockState oldState) {
        return oldState.getBlock() == asBlock();
    }
    protected BlockState rotate(BlockState state, Rotation rotation) {
        return state;
    }
    protected BlockState mirror(BlockState state, Mirror mirror) {
        return state;
    }
    protected boolean canBeReplaced(BlockState state, BlockPlaceContext context) {
        return state.canBeReplaced() && (context.getItemInHand().isEmpty() || !context.getItemInHand().is(asItem()));
    }
    protected boolean canBeReplaced(BlockState state, Fluid fluid) {
        return state.canBeReplaced() || !state.isSolid();
    }
    protected List<ItemStack> getDrops(BlockState state, LootParams.Builder params) {
        return List.of();
    }
    protected long getSeed(BlockState state, BlockPos pos) {
        return state.getSeed(pos);
    }
    protected VoxelShape getOcclusionShape(BlockState state) {
        return state.getOcclusionShape();
    }
    protected VoxelShape getBlockSupportShape(BlockState state, BlockGetter level, BlockPos pos) {
        return state.getBlockSupportShape(level, pos);
    }
    protected VoxelShape getInteractionShape(BlockState state, BlockGetter level, BlockPos pos) {
        return VoxelShape.EMPTY;
    }
    protected int getLightDampening(BlockState state) {
        return state.getLightDampening();
    }
    protected MenuProvider getMenuProvider(BlockState state, Level level, BlockPos pos) {
        return null;
    }
    protected boolean canSurvive(BlockState state, LevelReader level, BlockPos pos) {
        String world = level instanceof ServerLevel server ? server.getWorld().getName() : null;
        return asBlock().blockIndex() < 0 || world == null
            || org.basaltmc.bukkit.WorldEnvNatives.survives(world, pos.getX(), pos.getY(), pos.getZ(), state.stateId());
    }
    protected float getShadeBrightness(BlockState state, BlockGetter level, BlockPos pos) {
        return state.getShadeBrightness(level, pos);
    }
    protected int getAnalogOutputSignal(BlockState state, Level level, BlockPos pos, Direction direction) {
        return 0;
    }
    protected VoxelShape getShape(BlockState state, BlockGetter level, BlockPos pos, CollisionContext context) {
        return state.getShape(level, pos, context);
    }
    protected VoxelShape getCollisionShape(BlockState state, BlockGetter level, BlockPos pos, CollisionContext context) {
        return state.getCollisionShape(level, pos, context);
    }
    protected VoxelShape getEntityInsideCollisionShape(BlockState state, BlockGetter level, BlockPos pos, Entity entity) {
        return state.getCollisionShape(level, pos);
    }
    protected boolean isCollisionShapeFullBlock(BlockState state, BlockGetter level, BlockPos pos) {
        return state.isCollisionShapeFullBlock(level, pos);
    }
    protected VoxelShape getVisualShape(BlockState state, BlockGetter level, BlockPos pos, CollisionContext context) {
        return state.getVisualShape(level, pos, context);
    }
    protected void randomTick(BlockState state, ServerLevel level, BlockPos pos, RandomSource random) {
        if (asBlock().blockIndex() >= 0) {
            org.basaltmc.bukkit.RestNatives.randomTick(level.getWorld().getName(), pos.getX(), pos.getY(), pos.getZ());
        } else {
            tick(state, level, pos, random);
        }
    }
    protected void tick(BlockState state, ServerLevel level, BlockPos pos, RandomSource random) {
        if (asBlock().blockIndex() >= 0) {
            org.basaltmc.bukkit.WorldNatives.scheduleTick(level.getWorld().getName(), pos.getX(), pos.getY(), pos.getZ(), 0, false);
        }

    }
    protected float getDestroyProgress(BlockState state, Player player, BlockGetter level, BlockPos pos) {
        float hardness = state.getDestroySpeed(level, pos);
        if (hardness == -1.0F) {
            return 0.0F;
        }
        return player.getDestroySpeed(state) / hardness / (player.hasCorrectToolForDrops(state) ? 30 : 100);
    }
    protected void spawnAfterBreak(BlockState state, ServerLevel level, BlockPos pos, ItemStack tool, boolean dropExperience) {

    }
    protected void attack(BlockState state, Level level, BlockPos pos, Player player) {

    }
    protected boolean isSignalSource(BlockState state) {
        return false;
    }
    protected boolean shouldRedstoneWireConnectTo(BlockState state, BlockGetter level, BlockPos pos, Direction direction) {
        return false;
    }
    protected int getSignal(BlockState state, BlockGetter level, BlockPos pos, Direction direction) {
        return 0;
    }
    protected int ownSignal(BlockState state, BlockGetter level, BlockPos pos) {
        return 0;
    }
    protected void entityInside(BlockState state, Level level, BlockPos pos, Entity entity, InsideBlockEffectApplier effectApplier, boolean isPrecise) {

    }
    protected int getDirectSignal(BlockState state, BlockGetter level, BlockPos pos, Direction direction) {
        return 0;
    }
    public Optional<ResourceKey<LootTable>> getLootTable() {
        net.minecraft.resources.Identifier key = asBlock().blockKey();
        return Optional.of(ResourceKey.create((ResourceKey) net.minecraft.core.registries.Registries.LOOT_TABLE,
            net.minecraft.resources.Identifier.fromNamespaceAndPath(key.getNamespace(), "blocks/" + key.getPath())));
    }
    public String getDescriptionId() {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.block.state.BlockBehaviour#getDescriptionId");
    }
    protected void onProjectileHit(Level level, BlockState state, BlockHitResult blockHit, Projectile projectile) {

    }
    protected boolean propagatesSkylightDown(BlockState state) {
        return state.getFluidState().isEmpty() && !state.isSolidRender();
    }
    protected boolean isRandomlyTicking(BlockState state) {
        return state.isRandomlyTicking();
    }
    protected SoundType getSoundType(BlockState state) {
        return state.getSoundType();
    }
    protected ItemStack getCloneItemStack(LevelReader level, BlockPos pos, BlockState state, boolean includeData) {
        return new ItemStack(asItem());
    }
    public Item asItem() {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.block.state.BlockBehaviour#asItem");
    }
    protected Block asBlock() {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.block.state.BlockBehaviour#asBlock");
    }
    public MapColor defaultMapColor() {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.block.state.BlockBehaviour#defaultMapColor");
    }
    public float defaultDestroyTime() {
        return properties().destroyTime;
    }
    public static enum OffsetType {
        NONE, XZ, XYZ;
    }
    public static class Properties {
        public Function<BlockState, MapColor> mapColor = state -> MapColor.NONE;
        public boolean hasCollision = true;
        public SoundType soundType = SoundType.STONE;
        public ToIntFunction<BlockState> lightEmission = state -> 0;
        public float explosionResistance;
        public float destroyTime;
        public boolean requiresCorrectToolForDrops;
        public boolean isRandomlyTicking;
        public float friction = 0.6F;
        public float speedFactor = 1.0F;
        public float jumpFactor = 1.0F;
        public float bounceRestitution;
        public float fallDistanceReduction;
        public ResourceKey<Block> id;
        public Optional<ResourceKey<LootTable>> drops;
        public boolean noLootTable;
        public String descriptionId;
        public boolean canOcclude = true;
        public boolean isAir;
        public boolean ignitedByLava;
        public boolean liquid;
        public boolean forceSolidOff;
        public boolean forceSolidOn;
        public PushReaction pushReaction = PushReaction.PUSH_PULL;
        public boolean spawnTerrainParticles = true;
        public NoteBlockInstrument instrument = NoteBlockInstrument.HARP;
        public boolean replaceable;
        public StateArgumentPredicate<EntityType<?>> isValidSpawn = (state, level, pos, type) -> true;
        public StatePredicate isRedstoneConductor = (state, level, pos) -> true;
        public StatePredicate isSuffocating = (state, level, pos) -> true;
        public StateArgumentPredicate<AABB> isViewBlocking = (state, level, pos, box) -> true;
        public PostProcess postProcess = (state, level, pos) -> null;
        public Predicate<BlockState> emissiveRendering = state -> false;
        public boolean dynamicShape;
        public FeatureFlagSet requiredFeatures;
        public OffsetType offsetType = OffsetType.NONE;

        private Properties() {
        }
        public static Properties of() {
            return new Properties();
        }
        public static Properties ofFullCopy(BlockBehaviour block) {
            Properties copy = ofLegacyCopy(block);
            Properties source = block.properties;
            if (source != null) {
                copy.requiresCorrectToolForDrops = source.requiresCorrectToolForDrops;
                copy.isRandomlyTicking = source.isRandomlyTicking;
                copy.lightEmission = source.lightEmission;
                copy.mapColor = source.mapColor;
                copy.canOcclude = source.canOcclude;
                copy.isAir = source.isAir;
                copy.ignitedByLava = source.ignitedByLava;
                copy.liquid = source.liquid;
                copy.forceSolidOff = source.forceSolidOff;
                copy.forceSolidOn = source.forceSolidOn;
                copy.pushReaction = source.pushReaction;
                copy.instrument = source.instrument;
                copy.replaceable = source.replaceable;
                copy.isValidSpawn = source.isValidSpawn;
                copy.isRedstoneConductor = source.isRedstoneConductor;
                copy.isSuffocating = source.isSuffocating;
                copy.isViewBlocking = source.isViewBlocking;
                copy.postProcess = source.postProcess;
                copy.emissiveRendering = source.emissiveRendering;
                copy.offsetType = source.offsetType;
                copy.spawnTerrainParticles = source.spawnTerrainParticles;
                copy.requiredFeatures = source.requiredFeatures;
                copy.drops = source.drops;
                copy.noLootTable = source.noLootTable;
                copy.descriptionId = source.descriptionId;
            }
            return copy;
        }
        public static Properties ofLegacyCopy(BlockBehaviour block) {
            Properties copy = new Properties();
            copy.destroyTime = block.properties != null ? block.properties.destroyTime
                : block instanceof Block real ? (float) real.facts()[0] : 0.0F;
            if (block instanceof Block real && block.properties == null) {
                copy.explosionResistance = real.getExplosionResistance();
                copy.friction = real.getFriction();
                copy.speedFactor = real.getSpeedFactor();
                copy.jumpFactor = real.getJumpFactor();
                return copy;
            }
            copy.explosionResistance = block.explosionResistance;
            copy.hasCollision = block.hasCollision;
            copy.isRandomlyTicking = block.isRandomlyTicking;
            copy.soundType = block.soundType;
            copy.friction = block.friction;
            copy.speedFactor = block.speedFactor;
            copy.jumpFactor = block.jumpFactor;
            copy.bounceRestitution = block.bounceRestitution;
            copy.fallDistanceReduction = block.fallDistanceReduction;
            copy.dynamicShape = block.dynamicShape;
            return copy;
        }
        public Properties mapColor(DyeColor dyeColor) {
            MapColor color = dyeColor.getMapColor();
            this.mapColor = state -> color;
            return this;
        }
        public Properties mapColor(MapColor mapColor) {
            this.mapColor = state -> mapColor;
            return this;
        }
        public Properties mapColor(Function<BlockState, MapColor> mapColor) {
            this.mapColor = mapColor;
            return this;
        }
        public Properties noCollision() {
            this.hasCollision = false;
            this.canOcclude = false;
            return this;
        }
        public Properties noOcclusion() {
            this.canOcclude = false;
            return this;
        }
        public Properties friction(float friction) {
            this.friction = friction;
            return this;
        }
        public Properties speedFactor(float speedFactor) {
            this.speedFactor = speedFactor;
            return this;
        }
        public Properties jumpFactor(float jumpFactor) {
            this.jumpFactor = jumpFactor;
            return this;
        }
        public Properties bounceRestitution(float bounceRestitution) {
            this.bounceRestitution = bounceRestitution;
            return this;
        }
        public Properties fallDistanceReduction(float fallDistanceReduction) {
            this.fallDistanceReduction = fallDistanceReduction;
            return this;
        }
        public Properties sound(SoundType soundType) {
            this.soundType = soundType;
            return this;
        }
        public Properties lightLevel(ToIntFunction<BlockState> lightEmission) {
            this.lightEmission = lightEmission;
            return this;
        }
        public Properties strength(float destroyTime, float explosionResistance) {
            return destroyTime(destroyTime).explosionResistance(explosionResistance);
        }
        public Properties instabreak() {
            return strength(0.0F);
        }
        public Properties strength(float destroyTime) {
            return strength(destroyTime, destroyTime);
        }
        public Properties randomTicks() {
            this.isRandomlyTicking = true;
            return this;
        }
        public Properties dynamicShape() {
            this.dynamicShape = true;
            return this;
        }
        public Properties noLootTable() {
            this.noLootTable = true;
            this.drops = Optional.empty();
            return this;
        }
        public Properties overrideLootTable(Optional<ResourceKey<LootTable>> table) {
            this.drops = table;
            return this;
        }
        protected Optional<ResourceKey<LootTable>> effectiveDrops() {
            if (drops != null) {
                return drops;
            }
            if (id == null) {
                return Optional.empty();
            }
            ResourceKey<LootTable> table = ResourceKey.create(
                (ResourceKey) net.minecraft.core.registries.Registries.LOOT_TABLE, id.identifier().withPrefix("blocks/"));
            return Optional.of(table);
        }
        public Properties ignitedByLava() {
            this.ignitedByLava = true;
            return this;
        }
        public Properties liquid() {
            this.liquid = true;
            return this;
        }
        public Properties forceSolidOn() {
            this.forceSolidOn = true;
            return this;
        }
        public Properties forceSolidOff() {
            this.forceSolidOff = true;
            return this;
        }
        public Properties pushReaction(PushReaction pushReaction) {
            this.pushReaction = pushReaction;
            return this;
        }
        public Properties air() {
            this.isAir = true;
            return this;
        }
        public Properties isValidSpawn(StateArgumentPredicate<EntityType<?>> isValidSpawn) {
            this.isValidSpawn = isValidSpawn;
            return this;
        }
        public Properties isRedstoneConductor(StatePredicate isRedstoneConductor) {
            this.isRedstoneConductor = isRedstoneConductor;
            return this;
        }
        public Properties isSuffocating(StatePredicate isSuffocating) {
            this.isSuffocating = isSuffocating;
            return this;
        }
        public Properties isViewBlocking(StateArgumentPredicate<AABB> isViewBlocking) {
            this.isViewBlocking = isViewBlocking;
            return this;
        }
        public Properties postProcess(PostProcess postProcess) {
            this.postProcess = postProcess;
            return this;
        }
        public Properties emissiveRendering(Predicate<BlockState> emissiveRendering) {
            this.emissiveRendering = emissiveRendering;
            return this;
        }
        public Properties requiresCorrectToolForDrops() {
            this.requiresCorrectToolForDrops = true;
            return this;
        }
        public Properties destroyTime(float destroyTime) {
            this.destroyTime = destroyTime;
            return this;
        }
        public Properties explosionResistance(float explosionResistance) {
            this.explosionResistance = Math.max(0.0F, explosionResistance);
            return this;
        }
        public Properties offsetType(OffsetType offsetType) {
            this.offsetType = offsetType;
            return this;
        }
        public Properties noTerrainParticles() {
            this.spawnTerrainParticles = false;
            return this;
        }
        public Properties requiredFeatures(FeatureFlag... flags) {
            this.requiredFeatures = flags.length == 0 ? FeatureFlagSet.of() : FeatureFlagSet.of(flags[0], java.util.Arrays.copyOfRange(flags, 1, flags.length));
            return this;
        }
        public Properties instrument(NoteBlockInstrument instrument) {
            this.instrument = instrument;
            return this;
        }
        public Properties replaceable() {
            this.replaceable = true;
            return this;
        }
        public Properties setId(ResourceKey<Block> id) {
            this.id = id;
            return this;
        }
        public Properties overrideDescription(String descriptionId) {
            this.descriptionId = descriptionId;
            return this;
        }
        protected String effectiveDescriptionId() {
            if (descriptionId != null) {
                return descriptionId;
            }
            return id == null ? null : net.minecraft.util.Util.makeDescriptionId("block", id.identifier());
        }
    }
    public interface OffsetFunction {
        Vec3 evaluate(BlockState state, BlockPos pos);
    }
    public interface PostProcess {
        BlockPos getPostProcessPos(BlockState state, BlockGetter level, BlockPos pos);
    }
    public interface StateArgumentPredicate<A> {
        boolean test(BlockState state, BlockGetter level, BlockPos pos, A a);
    }
    public interface StatePredicate {
        boolean test(BlockState state, BlockGetter level, BlockPos pos);
    }
    public BlockBehaviour() {}
}
