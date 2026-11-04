// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.block;

import java.util.Iterator;
import java.util.List;
import net.minecraft.core.BlockPos;
import net.minecraft.core.IdMapper;
import net.minecraft.resources.Identifier;
import net.minecraft.world.item.context.BlockPlaceContext;
import net.minecraft.world.level.ItemLike;
import net.minecraft.world.level.LevelAccessor;
import net.minecraft.world.level.block.state.BlockBehaviour;
import net.minecraft.world.level.block.state.BlockState;
import net.minecraft.world.level.block.state.StateDefinition;
import net.minecraft.world.level.block.state.properties.Property;
import net.minecraft.world.level.material.MapColor;
import net.minecraft.world.phys.shapes.VoxelShape;
import org.basaltmc.bukkit.Registries;
import org.basaltmc.bukkit.WorldNatives;
import org.basaltmc.bukkit.facade.PropertyPool;
import java.util.function.BiConsumer;
import java.util.function.Function;
import java.util.function.IntFunction;
import net.minecraft.core.Direction;
import net.minecraft.core.Holder;
import net.minecraft.core.registries.BuiltInRegistries;
import net.minecraft.network.chat.Component;
import net.minecraft.network.chat.MutableComponent;
import net.minecraft.resources.ResourceKey;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.server.level.ServerPlayer;
import net.minecraft.util.RandomSource;
import net.minecraft.util.valueproviders.IntProvider;
import net.minecraft.world.entity.Entity;
import net.minecraft.world.entity.LivingEntity;
import net.minecraft.world.entity.player.Player;
import net.minecraft.world.item.Item;
import net.minecraft.world.item.ItemInstance;
import net.minecraft.world.item.ItemStack;
import net.minecraft.world.level.BlockGetter;
import net.minecraft.world.level.Explosion;
import net.minecraft.world.level.Level;
import net.minecraft.world.level.LevelReader;
import net.minecraft.world.level.biome.Biome;
import net.minecraft.world.level.block.entity.BlockEntity;
import net.minecraft.world.level.storage.loot.LootParams;
import net.minecraft.world.level.storage.loot.LootTable;

public class Block extends BlockBehaviour implements ItemLike {
    public static final IdMapper<BlockState> BLOCK_STATE_REGISTRY = new StateIds();
    private static volatile Block[] kBlocks;

    private final int index;
    private final Identifier key;
    private final int first;
    private final int count;
    private volatile Property<?>[] stateProperties;
    private int[] strides;
    private volatile BlockState[] states;
    private StateDefinition<Block, BlockState> definition;
    private double[] facts;
    private double[] info;

    protected Block(int index) {
        this.index = index;
        this.key = Identifier.parse(Registries.blockName(index));
        this.first = Registries.firstState(index);
        this.count = Registries.stateCount(index);
    }

    public static Block byIndex(int index) {
        return blocks()[index];
    }

    public static int blockCount() {
        return blocks().length;
    }

    private static Block[] blocks() {
        Block[] found = kBlocks;
        if (found == null) {
            synchronized (Block.class) {
                found = kBlocks;
                if (found == null) {
                    Block[] made = new Block[Registries.blockCount()];
                    for (int i = 0; i < made.length; i++) {
                        boolean tile = (WorldNatives.stateFlags(Registries.firstState(i)) & (1 << 18)) != 0;
                        Block typed = tile ? BlockClasses.createEntity(i, Registries.blockName(i)) : null;
                        if (typed == null) {
                            typed = BlockClasses.create(i, Registries.blockName(i));
                        }
                        made[i] = tile && !(typed instanceof EntityBlock) ? new EntityBlockImpl(i) : typed;
                    }
                    kBlocks = found = made;
                }
            }
        }
        return found;
    }

    private static final class EntityBlockImpl extends Block implements EntityBlock {
        private EntityBlockImpl(int index) {
            super(index);
        }

        @Override
        public net.minecraft.world.level.block.entity.BlockEntity newBlockEntity(net.minecraft.core.BlockPos pos,
                                                                                  net.minecraft.world.level.block.state.BlockState state) {
            return org.basaltmc.bukkit.facade.BlockEntityTypes.detached(blockKey(), pos);
        }
    }

    public int blockIndex() {
        return index;
    }

    public Identifier blockKey() {
        return key;
    }

    public int firstState() {
        return first;
    }

    private void loadProperties() {
        if (index < 0 && stateProperties == null) {
            strides = new int[0];
            stateProperties = new Property<?>[0];
        }
        if (stateProperties == null) {
            synchronized (this) {
                if (stateProperties == null) {
                    Registries.Properties table = Registries.propertiesOf(index);
                    Property<?>[] made = new Property<?>[table.names().length];
                    for (int i = 0; i < made.length; i++) {
                        made[i] = PropertyPool.of(table.names()[i], table.values()[i]);
                    }
                    strides = table.strides();
                    stateProperties = made;
                }
            }
        }
    }

    public Property<?>[] propertiesInOrder() {
        loadProperties();
        return stateProperties;
    }

    public int strideOf(int propertyIndex) {
        loadProperties();
        return strides[propertyIndex];
    }

    public int indexOf(Property<?> property) {
        Property<?>[] all = propertiesInOrder();
        for (int i = 0; i < all.length; i++) {
            if (all[i] == property) {
                return i;
            }
        }
        return -1;
    }

    public BlockState stateAtOffset(int offset) {
        BlockState[] all = states();
        return offset >= 0 && offset < all.length ? all[offset] : null;
    }

    private BlockState[] states() {
        BlockState[] found = states;
        if (found == null) {
            synchronized (this) {
                found = states;
                if (found == null) {
                    BlockState[] made = new BlockState[count];
                    for (int i = 0; i < count; i++) {
                        made[i] = new BlockState(this, first + i);
                    }
                    states = found = made;
                }
            }
        }
        return found;
    }

    @Override
    public BlockBehaviour.Properties properties() {
        BlockBehaviour.Properties found = properties;
        if (found == null) {
            found = BlockBehaviour.Properties.ofLegacyCopy(this).mapColor(defaultMapColor());
            properties = found;
        }
        return found;
    }

    public double[] facts() {
        if (facts == null && index < 0) {
            facts = new double[] {properties.destroyTime, properties.explosionResistance, properties.requiresCorrectToolForDrops ? 1.0 : 0.0};
        }
        if (facts == null) {
            double[] found = WorldNatives.blockHardness(key.toString());
            facts = found == null || found.length < 3 ? new double[] {0.0, 0.0, 0.0} : found;
        }
        return facts;
    }

    public double[] info() {
        if (info == null && index < 0) {
            info = new double[] {properties.friction, properties.speedFactor, properties.jumpFactor, 0.0,
                properties.mapColor.apply(defaultBlockState()).id, 0.0, 0.0};
        }
        if (info == null) {
            double[] found = WorldNatives.blockInfo(first);
            info = found == null || found.length < 7 ? new double[] {0.6, 1.0, 1.0, 0.0, 0.0, 0.0, 0.0} : found;
        }
        return info;
    }

    public BlockState defaultBlockState() {
        if (index < 0) {
            return stateAtOffset(0);
        }
        return stateById(Registries.defaultState(index));
    }

    public StateDefinition<Block, BlockState> getStateDefinition() {
        StateDefinition<Block, BlockState> found = definition;
        if (found == null) {
            found = new StateDefinition<>(this, List.of(propertiesInOrder()), List.of(states()));
            definition = found;
        }
        return found;
    }

    public static BlockState stateById(int idWithData) {
        int block = Registries.blockOfState(idWithData);
        if (block < 0) {
            return null;
        }
        Block owner = byIndex(block);
        return owner.stateAtOffset(idWithData - owner.first);
    }

    public static int getId(BlockState state) {
        return state == null ? -1 : state.stateId();
    }

    public BlockState getStateForPlacement(BlockPlaceContext context) {
        return defaultBlockState();
    }

    public static BlockState updateFromNeighbourShapes(BlockState state, LevelAccessor level, BlockPos pos) {
        return org.basaltmc.bukkit.facade.Effects.valid(level, pos, state);
    }

    public static boolean isShapeFullBlock(VoxelShape shape) {
        return shape != null && shape.isFullBlock();
    }

    public MapColor defaultMapColor() {
        return new MapColor((int) info()[4]);
    }

    public String getDescriptionId() {
        return "block." + key.getNamespace() + "." + key.getPath().replace('/', '.');
    }

    public float getExplosionResistance() {
        return (float) facts()[1];
    }

    public float getFriction() {
        return (float) info()[0];
    }

    public float getSpeedFactor() {
        return (float) info()[1];
    }

    public float getJumpFactor() {
        return (float) info()[2];
    }

    @Override
    public String toString() {
        return "Block{" + key + "}";
    }

    private static final class StateIds extends IdMapper<BlockState> {
        @Override
        public int getId(BlockState state) {
            return state == null ? -1 : state.stateId();
        }

        @Override
        public BlockState byId(int id) {
            return stateById(id);
        }

        @Override
        public int size() {
            int last = Registries.blockCount() - 1;
            return Registries.firstState(last) + Registries.stateCount(last);
        }

        @Override
        public Iterator<BlockState> iterator() {
            return new Iterator<>() {
                private int next;

                @Override
                public boolean hasNext() {
                    return next < size();
                }

                @Override
                public BlockState next() {
                    return stateById(next++);
                }
            };
        }
    }
    public static final int UPDATE_NEIGHBORS = 1;
    public static final int UPDATE_CLIENTS = 2;
    public static final int UPDATE_INVISIBLE = 4;
    public static final int UPDATE_IMMEDIATE = 8;
    public static final int UPDATE_KNOWN_SHAPE = 16;
    public static final int UPDATE_SUPPRESS_DROPS = 32;
    public static final int UPDATE_MOVE_BY_PISTON = 64;
    public static final int UPDATE_SKIP_SHAPE_UPDATE_ON_WIRE = 128;
    public static final int UPDATE_SKIP_BLOCK_ENTITY_SIDEEFFECTS = 256;
    public static final int UPDATE_SKIP_ON_PLACE = 512;
    public static final int UPDATE_NONE = 260;
    public static final int UPDATE_ALL = 3;
    public static final int UPDATE_ALL_IMMEDIATE = 11;
    public static final int UPDATE_SKIP_ALL_SIDEEFFECTS = 816;
    public static final float INDESTRUCTIBLE = -1.0F;
    public static final float INSTANT = 0.0F;
    public static final int UPDATE_LIMIT = 512;
    protected final StateDefinition<Block, BlockState> stateDefinition = null;
    public static Block byItem(Item item) {
        int index = org.basaltmc.bukkit.Registries.blockIndex(item.itemKey().toString());
        return index < 0 ? Blocks.AIR : Block.byIndex(index);
    }
    public static BlockState pushEntitiesUp(BlockState state, BlockState newState, LevelAccessor level, BlockPos pos) {
        if (!(level instanceof net.minecraft.server.level.ServerLevel server)) {
            return newState;
        }
        VoxelShape before = state.getCollisionShape(level, pos);
        for (net.minecraft.world.phys.AABB box : newState.getCollisionShape(level, pos).toAabbs()) {
            if (before.toAabbs().contains(box)) {
                continue;
            }
            org.bukkit.util.BoundingBox area = new org.bukkit.util.BoundingBox(pos.getX() + box.minX, pos.getY() + box.minY,
                pos.getZ() + box.minZ, pos.getX() + box.maxX, pos.getY() + box.maxY, pos.getZ() + box.maxZ);
            for (org.bukkit.entity.Entity found : server.getWorld().getNearbyEntities(area)) {
                double lift = area.getMaxY() - found.getBoundingBox().getMinY();
                if (lift > 0.0 && lift <= 1.0) {
                    found.teleport(found.getLocation().add(0.0, lift, 0.0));
                }
            }
        }
        return newState;
    }
    public static VoxelShape box(double minX, double minY, double minZ, double maxX, double maxY, double maxZ) {
        return VoxelShape.ofBoxes(List.of(new net.minecraft.world.phys.AABB(minX / 16.0, minY / 16.0, minZ / 16.0, maxX / 16.0, maxY / 16.0, maxZ / 16.0)));
    }
    public static VoxelShape[] boxes(int endInclusive, IntFunction<VoxelShape> voxelShapeFactory) {
        VoxelShape[] shapes = new VoxelShape[endInclusive + 1];
        for (int i = 0; i <= endInclusive; i++) {
            shapes[i] = voxelShapeFactory.apply(i);
        }
        return shapes;
    }
    public static VoxelShape cube(double size) {
        return cube(size, size, size);
    }
    public static VoxelShape cube(double sizeX, double sizeY, double sizeZ) {
        return column(sizeX, sizeZ, 8.0 - sizeY / 2.0, 8.0 + sizeY / 2.0);
    }
    public static VoxelShape column(double sizeXZ, double minY, double maxY) {
        return column(sizeXZ, sizeXZ, minY, maxY);
    }
    public static VoxelShape column(double sizeX, double sizeZ, double minY, double maxY) {
        return box(8.0 - sizeX / 2.0, minY, 8.0 - sizeZ / 2.0, 8.0 + sizeX / 2.0, maxY, 8.0 + sizeZ / 2.0);
    }
    public static VoxelShape boxZ(double sizeXY, double minZ, double maxZ) {
        return boxZ(sizeXY, sizeXY, minZ, maxZ);
    }
    public static VoxelShape boxZ(double sizeX, double sizeY, double minZ, double maxZ) {
        return boxZ(sizeX, 8.0 - sizeY / 2.0, 8.0 + sizeY / 2.0, minZ, maxZ);
    }
    public static VoxelShape boxZ(double sizeX, double minY, double maxY, double minZ, double maxZ) {
        return box(8.0 - sizeX / 2.0, minY, minZ, 8.0 + sizeX / 2.0, maxY, maxZ);
    }
    public static void updateOrDestroy(BlockState blockState, BlockState newState, LevelAccessor level, BlockPos blockPos, int updateFlags) {
        updateOrDestroy(blockState, newState, level, blockPos, updateFlags, 512);
    }
    public static void updateOrDestroy(BlockState blockState, BlockState newState, LevelAccessor level, BlockPos blockPos, int updateFlags, int updateLimit) {
        if (newState == blockState) {
            return;
        }
        if (newState.isAir()) {
            level.destroyBlock(blockPos, (updateFlags & 32) == 0, null, updateLimit);
        } else {
            level.setBlock(blockPos, newState, updateFlags & -33, updateLimit);
        }
    }
    public Block(BlockBehaviour.Properties properties) {
        super(properties);
        this.index = -1;
        this.key = properties.id != null ? properties.id.identifier() : Identifier.parse("detached:block");
        this.first = -1;
        this.count = 1;
    }
    public static boolean isExceptionForConnection(BlockState state) {
        return state.getBlock() instanceof LeavesBlock || state.is(Blocks.BARRIER) || state.is(Blocks.CARVED_PUMPKIN)
            || state.is(Blocks.JACK_O_LANTERN) || state.is(Blocks.MELON) || state.is(Blocks.PUMPKIN)
            || state.is(net.minecraft.tags.BlockTags.SHULKER_BOXES);
    }
    public static boolean dropFromBlockInteractLootTable(ServerLevel level, ResourceKey<LootTable> key, BlockPos interactedBlockPos, BlockState interactedBlockState, BlockEntity interactedBlockEntity, ItemInstance tool, Entity interactingEntity, BiConsumer<ServerLevel, ItemStack> consumer) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.block.Block#dropFromBlockInteractLootTable");
    }
    protected static boolean dropFromLootTable(ServerLevel level, ResourceKey<LootTable> key, Function<LootParams.Builder, LootParams> paramsBuilder, BiConsumer<ServerLevel, ItemStack> consumer) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.block.Block#dropFromLootTable");
    }
    public static boolean shouldRenderFace(BlockState state, BlockState neighborState, Direction direction) {
        VoxelShape occluder = neighborState.getFaceOcclusionShape(direction.getOpposite());
        if (occluder.isFullBlock()) {
            return false;
        }
        if (state.skipRendering(neighborState, direction)) {
            return false;
        }
        VoxelShape face = state.getFaceOcclusionShape(direction);
        if (occluder.isEmpty() || face.isEmpty()) {
            return true;
        }
        for (net.minecraft.world.phys.AABB part : face.toAabbs()) {
            boolean covered = false;
            for (net.minecraft.world.phys.AABB cover : occluder.toAabbs()) {
                covered |= cover.minX <= part.minX && cover.minY <= part.minY && cover.minZ <= part.minZ
                    && cover.maxX >= part.maxX && cover.maxY >= part.maxY && cover.maxZ >= part.maxZ;
            }
            if (!covered) {
                return true;
            }
        }
        return false;
    }
    public static boolean canSupportRigidBlock(BlockGetter level, BlockPos below) {
        return level.getBlockState(below).isFaceSturdy(level, below, Direction.UP, SupportType.RIGID);
    }
    public static boolean canSupportCenter(LevelReader level, BlockPos belowPos, Direction direction) {
        BlockState support = level.getBlockState(belowPos);
        if (direction == Direction.DOWN && support.is(net.minecraft.tags.BlockTags.UNSTABLE_BOTTOM_CENTER)) {
            return false;
        }
        return support.isFaceSturdy(level, belowPos, direction, SupportType.CENTER);
    }
    public static boolean isFaceFull(VoxelShape shape, Direction direction) {
        return isShapeFullBlock(shape.getFaceShape(direction));
    }
    public void animateTick(BlockState state, Level level, BlockPos pos, RandomSource random) {

    }
    public void destroy(LevelAccessor level, BlockPos pos, BlockState state) {

    }
    public static List<ItemStack> getDrops(BlockState state, ServerLevel level, BlockPos pos, BlockEntity blockEntity) {
        return getDrops(state, level, pos, blockEntity, null, null);
    }
    public static List<ItemStack> getDrops(BlockState state, ServerLevel level, BlockPos pos, BlockEntity blockEntity, Entity breaker, ItemInstance tool) {
        String[] flat = org.basaltmc.bukkit.WorldEnvNatives.blockDrops(level.getWorld().getName(), pos.getX(), pos.getY(),
            pos.getZ(), tool instanceof ItemStack stack && !stack.isEmpty() ? stack.getItem().itemKey().toString() : "",
            new String[0], new int[0], breaker != null);
        java.util.List<ItemStack> drops = new java.util.ArrayList<>();
        if (flat != null) {
            for (int i = 0; i + 2 < flat.length; i += 3) {
                net.minecraft.world.item.Item item = net.minecraft.core.registries.BuiltInRegistries.ITEM.getOptional(
                    net.minecraft.resources.Identifier.parse(flat[i])).orElse(null);
                if (item != null) {
                    drops.add(new ItemStack(item, Integer.parseInt(flat[i + 1])));
                }
            }
        }
        return drops;
    }
    public static void dropResources(BlockState state, Level level, BlockPos pos) {
        if (level instanceof net.minecraft.server.level.ServerLevel server) {
            for (ItemStack stack : getDrops(state, server, pos, null)) {
                popResource(level, pos, stack);
            }
        }
    }
    public static void dropResources(BlockState state, LevelAccessor level, BlockPos pos, BlockEntity blockEntity) {
        if (level instanceof net.minecraft.server.level.ServerLevel server) {
            for (ItemStack stack : getDrops(state, server, pos, blockEntity)) {
                popResource(server, pos, stack);
            }
            state.spawnAfterBreak(server, pos, ItemStack.EMPTY, true);
        }
    }
    public static void dropResources(BlockState state, Level level, BlockPos pos, BlockEntity blockEntity, Entity breaker, ItemStack tool) {
        if (level instanceof net.minecraft.server.level.ServerLevel server) {
            for (ItemStack stack : getDrops(state, server, pos, blockEntity, breaker, tool)) {
                popResource(server, pos, stack);
            }
            state.spawnAfterBreak(server, pos, tool, true);
        }
    }
    public static void popResource(Level level, BlockPos pos, ItemStack itemStack) {
        org.bukkit.World world = level instanceof net.minecraft.server.level.ServerLevel server ? server.getWorld() : null;
        if (world != null && !itemStack.isEmpty()) {
            world.dropItem(new org.bukkit.Location(world, pos.getX() + 0.5, pos.getY() + 0.5, pos.getZ() + 0.5),
                org.bukkit.craftbukkit.inventory.CraftItemStack.asCraftMirror(itemStack));
        }
    }
    public static void popResourceFromFace(Level level, BlockPos pos, Direction face, ItemStack itemStack) {
        org.bukkit.World world = level instanceof net.minecraft.server.level.ServerLevel server ? server.getWorld() : null;
        if (world == null || itemStack.isEmpty()) {
            return;
        }
        java.util.concurrent.ThreadLocalRandom random = java.util.concurrent.ThreadLocalRandom.current();
        int sx = face.getStepX();
        int sy = face.getStepY();
        int sz = face.getStepZ();
        double x = pos.getX() + 0.5 + (sx == 0 ? random.nextDouble(-0.25, 0.25) : sx * 0.625);
        double y = pos.getY() + 0.5 + (sy == 0 ? random.nextDouble(-0.25, 0.25) : sy * 0.625) - 0.125;
        double z = pos.getZ() + 0.5 + (sz == 0 ? random.nextDouble(-0.25, 0.25) : sz * 0.625);
        org.bukkit.entity.Item dropped = world.dropItem(new org.bukkit.Location(world, x, y, z),
            org.bukkit.craftbukkit.inventory.CraftItemStack.asCraftMirror(itemStack));
        dropped.setVelocity(new org.bukkit.util.Vector(sx == 0 ? random.nextDouble(-0.1, 0.1) : sx * 0.1,
            sy == 0 ? random.nextDouble(0.0, 0.1) : sy * 0.1 + 0.1, sz == 0 ? random.nextDouble(-0.1, 0.1) : sz * 0.1));
    }
    protected void popExperience(ServerLevel level, BlockPos pos, int amount) {
        if (amount > 0 && level.getWorld() != null) {
            org.bukkit.Location at = new org.bukkit.Location(level.getWorld(), pos.getX() + 0.5, pos.getY() + 0.5, pos.getZ() + 0.5);
            level.getWorld().spawn(at, org.bukkit.entity.ExperienceOrb.class, orb -> orb.setExperience(amount));
        }
    }
    public void wasExploded(ServerLevel level, BlockPos pos, Explosion explosion) {

    }
    public void stepOn(Level level, BlockPos pos, BlockState onState, Entity entity) {

    }
    public void playerDestroy(ServerLevel level, ServerPlayer player, BlockPos pos, BlockState state, BlockEntity blockEntity, ItemStack destroyedWith) {
        player.causeFoodExhaustion(0.005F);
        dropResources(state, level, pos, blockEntity, player, destroyedWith);
    }
    public void setPlacedBy(Level level, BlockPos pos, BlockState state, LivingEntity by, ItemStack itemStack) {

    }
    public boolean isPossibleToRespawnInThis(BlockState state) {
        return !state.isSolid() && !state.liquid();
    }
    public MutableComponent getName() {
        return Component.translatable(getDescriptionId());
    }
    public void fallOn(Level level, BlockState state, BlockPos pos, Entity entity, double fallDistance) {
        if (entity.getBukkitEntity() instanceof org.bukkit.entity.LivingEntity living) {
            double damage = Math.ceil(fallDistance * (1.0 - getFallDistanceReduction()) - 3.0);
            if (damage > 0.0) {
                living.damage(damage);
            }
        }
    }
    public void bounceOn(Level level, BlockState state, BlockPos pos, Entity entity, double fallDistance) {
        
    }
    public float getBounceRestitution() {
        return 0.0F;
    }
    public float getFallDistanceReduction() {
        return 1.0F;
    }
    public void spawnDestroyByEntityParticles(Level level, Entity entity, BlockPos pos, BlockState state) {

    }
    public void spawnDestroyParticles(Level level, BlockPos pos, BlockState state) {

    }
    public BlockState playerWillDestroy(Level level, BlockPos pos, BlockState state, Player player) {
        return state;
    }
    public void handlePrecipitation(BlockState state, Level level, BlockPos pos, Biome.Precipitation precipitation) {

    }
    public boolean dropFromExplosion(Explosion explosion) {
        return true;
    }
    protected void createBlockStateDefinition(StateDefinition.Builder<Block, BlockState> builder) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.block.Block#createBlockStateDefinition");
    }
    protected void registerDefaultState(BlockState state) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.block.Block#registerDefaultState");
    }
    private static <T extends Comparable<T>> BlockState copyProperty(BlockState from, BlockState to,
            net.minecraft.world.level.block.state.properties.Property<T> property) {
        return to.hasProperty(property) ? to.setValue(property, from.getValue(property)) : to;
    }
    public BlockState withPropertiesOf(BlockState source) {
        BlockState result = defaultBlockState();
        for (net.minecraft.world.level.block.state.properties.Property<?> property : source.getProperties()) {
            result = copyProperty(source, result, property);
        }
        return result;
    }
    public Item asItem() {
        return Item.byBlock(this);
    }
    public boolean hasDynamicShape() {
        return false;
    }
    protected Block asBlock() {
        return this;
    }
    protected Function<BlockState, VoxelShape> getShapeForEachState(Function<BlockState, VoxelShape> shapeCalculator) {
        return getShapeForEachState(shapeCalculator, new Property<?>[0]);
    }
    @SuppressWarnings({"unchecked", "rawtypes"})
    protected Function<BlockState, VoxelShape> getShapeForEachState(Function<BlockState, VoxelShape> shapeCalculator, Property<?>... ignoredProperties) {
        java.util.Map<BlockState, VoxelShape> cache = new java.util.concurrent.ConcurrentHashMap<>();
        BlockState base = defaultBlockState();
        return state -> {
            BlockState key = state;
            for (Property ignored : ignoredProperties) {
                if (key.hasProperty(ignored)) {
                    key = key.setValue(ignored, base.getValue(ignored));
                }
            }
            BlockState lookup = key;
            return cache.computeIfAbsent(lookup, shapeCalculator);
        };
    }
    public Holder.Reference<Block> builtInRegistryHolder() {
        return (Holder.Reference<Block>) BuiltInRegistries.BLOCK.wrapAsHolder(this);
    }
    protected void tryDropExperience(ServerLevel level, BlockPos pos, ItemStack tool, IntProvider xpRange) {
        if (!tool.isEmpty() && org.bukkit.craftbukkit.inventory.CraftItemStack.asCraftMirror(tool)
                .containsEnchantment(org.bukkit.enchantments.Enchantment.SILK_TOUCH)) {
            return;
        }
        popExperience(level, pos, xpRange.sample(net.minecraft.util.RandomSource.create()));
    }
    static record ShapePairKey(VoxelShape first, VoxelShape second) {
    }
    public @interface UpdateFlags {
    }
}
