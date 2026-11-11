// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.gametest.framework;

import java.util.ArrayList;
import java.util.List;
import java.util.Objects;
import java.util.Optional;
import java.util.function.Function;
import java.util.function.IntPredicate;
import java.util.function.Predicate;
import java.util.function.Supplier;
import net.minecraft.core.BlockPos;
import net.minecraft.core.Direction;
import net.minecraft.core.Holder;
import net.minecraft.network.chat.Component;
import net.minecraft.resources.ResourceKey;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.server.level.ServerPlayer;
import net.minecraft.world.damagesource.DamageSource;
import net.minecraft.world.effect.MobEffect;
import net.minecraft.world.entity.Entity;
import net.minecraft.world.entity.EntitySpawnReason;
import net.minecraft.world.entity.EntityType;
import net.minecraft.world.entity.LivingEntity;
import net.minecraft.world.entity.Mob;
import net.minecraft.world.entity.item.ItemEntity;
import net.minecraft.world.entity.player.Player;
import net.minecraft.world.item.Item;
import net.minecraft.world.item.ItemStack;
import net.minecraft.world.level.GameType;
import net.minecraft.world.level.biome.Biome;
import net.minecraft.world.level.block.Block;
import net.minecraft.world.level.block.Rotation;
import net.minecraft.world.level.block.entity.BlockEntity;
import net.minecraft.world.level.block.state.BlockState;
import net.minecraft.world.level.block.state.properties.Property;
import net.minecraft.world.level.levelgen.Heightmap;
import net.minecraft.world.level.levelgen.structure.BoundingBox;
import net.minecraft.world.phys.AABB;
import net.minecraft.world.phys.BlockHitResult;
import net.minecraft.world.phys.Vec3;
import org.basaltmc.bukkit.WorldNatives;

public abstract class GameTestHelperAssertions extends GameTestHelperSpace {
    protected GameTestHelperAssertions(GameTestInfo testInfo) {
        super(testInfo);
    }

    public void assertBlockPresent(Block blockType, int x, int y, int z) {
        assertBlockPresent(blockType, new BlockPos(x, y, z));
    }

    public void assertBlockPresent(Block blockType, BlockPos pos) {
        BlockState state = getBlockState(pos);
        assertBlock(pos, block -> state.is(blockType), block -> Component.translatable("test.error.expected_block", blockType.getName(), block.getName()));
    }

    public void assertBlockPresent(Block blockType) {
        boolean[] found = {false};
        forEveryBlockInStructure(pos -> found[0] |= getBlockState(pos).is(blockType));
        if (!found[0]) {
            throw assertionException("test.error.expected_block_anywhere", blockType.getName());
        }
    }

    public void assertBlockNotPresent(Block blockType, int x, int y, int z) {
        assertBlockNotPresent(blockType, new BlockPos(x, y, z));
    }

    public void assertBlockNotPresent(Block blockType, BlockPos pos) {
        assertBlock(pos, block -> !getBlockState(pos).is(blockType), block -> Component.translatable("test.error.unexpected_block", blockType.getName()));
    }

    public void assertBlockTag(net.minecraft.tags.TagKey<Block> tag, BlockPos pos) {
        assertBlockState(pos, state -> state.is(tag), state -> Component.translatable("test.error.expected_block_tag", tag.location().toString(), state.getBlock().getName()));
    }

    public void succeedWhenBlockPresent(Block block, int x, int y, int z) {
        succeedWhenBlockPresent(block, new BlockPos(x, y, z));
    }

    public void succeedWhenBlockPresent(Block block, BlockPos pos) {
        succeedWhen(() -> assertBlockPresent(block, pos));
    }

    public void assertBlock(BlockPos pos, Predicate<Block> predicate, Function<Block, Component> errorMessage) {
        assertBlockState(pos, state -> predicate.test(state.getBlock()), state -> errorMessage.apply(state.getBlock()));
    }

    public <T extends Comparable<T>> void assertBlockProperty(BlockPos pos, Property<T> property, T value) {
        BlockState state = getBlockState(pos);
        if (!state.hasProperty(property) || !state.getValue(property).equals(value)) {
            throw assertionException(pos, "test.error.expected_property", property.getName(), value, state.hasProperty(property) ? state.getValue(property) : "missing");
        }
    }

    public <T extends Comparable<T>> void assertBlockProperty(BlockPos pos, Property<T> property, Predicate<T> predicate, Component errorMessage) {
        assertBlockState(pos, state -> state.hasProperty(property) && predicate.test(state.getValue(property)), state -> errorMessage);
    }

    public void assertBlockState(BlockPos pos, BlockState expected) {
        BlockState actual = getBlockState(pos);
        if (!actual.equals(expected)) {
            throw assertionException(pos, "test.error.state_not_equal", expected, actual);
        }
    }

    public void assertBlockState(BlockPos pos, Predicate<BlockState> predicate, Function<BlockState, Component> errorMessage) {
        BlockState state = getBlockState(pos);
        if (!predicate.test(state)) {
            throw assertionException(pos, errorMessage.apply(state));
        }
    }

    public <T extends BlockEntity> void assertBlockEntityData(BlockPos pos, Class<T> type, Predicate<T> predicate, Supplier<Component> errorMessage) {
        if (!predicate.test(getBlockEntity(pos, type))) {
            throw assertionException(pos, errorMessage.get());
        }
    }

    public void assertRedstoneSignal(BlockPos pos, Direction direction, IntPredicate levelPredicate, Supplier<Component> errorMessage) {
        BlockPos abs = absolutePos(pos);
        int signal = getLevel().getSignal(abs, getAbsoluteDirection(direction));
        if (!levelPredicate.test(signal)) {
            throw assertionException(pos, errorMessage.get());
        }
    }

    public void assertEntityPresent(EntityType<?> entityType) {
        if (entitiesIn(entityType, getBounds()).isEmpty()) {
            throw assertionException("test.error.expected_entity_in_test", entityType.getDescription());
        }
    }

    public void assertEntityPresent(EntityType<?> entityType, int x, int y, int z) {
        assertEntityPresent(entityType, new BlockPos(x, y, z));
    }

    public void assertEntityPresent(EntityType<?> entityType, BlockPos pos) {
        if (entitiesIn(entityType, new AABB(absolutePos(pos))).isEmpty()) {
            throw assertionException(pos, "test.error.expected_entity", entityType.getDescription());
        }
    }

    public void assertEntityPresent(EntityType<?> entityType, AABB relativeAABB) {
        assertEntityPresent(entityType, relativeAABB, Component.translatable("test.error.expected_entity_in_box", entityType.getDescription()));
    }

    public void assertEntityPresent(EntityType<?> entityType, AABB relativeAABB, Component message) {
        if (entitiesIn(entityType, absoluteAABB(relativeAABB)).isEmpty()) {
            throw assertionException(message);
        }
    }

    public void assertEntitiesPresent(EntityType<?> entityType, int expectedEntities) {
        int actual = entitiesIn(entityType, getBounds()).size();
        if (actual != expectedEntities) {
            throw assertionException("test.error.expected_entity_count", expectedEntities, entityType.getDescription(), actual);
        }
    }

    public void assertEntitiesPresent(EntityType<?> entityType, BlockPos pos, int numOfExpectedEntities, double distance) {
        int actual = getEntities(entityType, pos, distance).size();
        if (actual != numOfExpectedEntities) {
            throw assertionException(pos, "test.error.expected_entity_count", numOfExpectedEntities, entityType.getDescription(), actual);
        }
    }

    public void assertEntityPresent(EntityType<?> entityType, BlockPos pos, double distance) {
        if (getEntities(entityType, pos, distance).isEmpty()) {
            throw assertionException(pos, "test.error.expected_entity", entityType.getDescription());
        }
    }

    public <T extends Entity> List<T> getEntities(EntityType<T> entityType, BlockPos pos, double distance) {
        return entitiesIn(entityType, new AABB(absolutePos(pos)).inflate(distance));
    }

    public <T extends Entity> List<T> getEntities(EntityType<T> entityType) {
        return entitiesIn(entityType, getBounds());
    }

    public void assertEntityInstancePresent(Entity entity, int x, int y, int z) {
        assertEntityInstancePresent(entity, new BlockPos(x, y, z));
    }

    public void assertEntityInstancePresent(Entity entity, BlockPos pos) {
        assertEntityInstancePresent(entity, pos, 0.0);
    }

    public void assertEntityInstancePresent(Entity entity, BlockPos pos, double inflate) {
        if (!new AABB(absolutePos(pos)).inflate(inflate).intersects(entity.getBoundingBox())) {
            throw assertionException(pos, "test.error.expected_entity", entity.getType().getDescription());
        }
    }

    private List<ItemEntity> itemsAt(Item item, AABB absolute) {
        return getLevel().getEntitiesOfClass(ItemEntity.class, absolute, e -> e.getItem().is(item));
    }

    public void assertItemEntityCountIs(Item itemType, BlockPos pos, double distance, int count) {
        int total = itemsAt(itemType, new AABB(absolutePos(pos)).inflate(distance)).stream().mapToInt(e -> e.getItem().getCount()).sum();
        if (total != count) {
            throw assertionException(pos, "test.error.expected_items_count", count, itemType.getDescriptionId(), total);
        }
    }

    public void assertItemEntityPresent(Item itemType, BlockPos pos, double distance) {
        if (itemsAt(itemType, new AABB(absolutePos(pos)).inflate(distance)).isEmpty()) {
            throw assertionException(pos, "test.error.expected_item", itemType.getDescriptionId());
        }
    }

    public void assertItemEntityNotPresent(Item itemType, BlockPos pos, double distance) {
        if (!itemsAt(itemType, new AABB(absolutePos(pos)).inflate(distance)).isEmpty()) {
            throw assertionException(pos, "test.error.unexpected_item", itemType.getDescriptionId());
        }
    }

    public void assertItemEntityPresent(Item itemType) {
        if (itemsAt(itemType, getBounds()).isEmpty()) {
            throw assertionException("test.error.expected_item", itemType.getDescriptionId());
        }
    }

    public void assertItemEntityNotPresent(Item itemType) {
        if (!itemsAt(itemType, getBounds()).isEmpty()) {
            throw assertionException("test.error.unexpected_item", itemType.getDescriptionId());
        }
    }

    public void assertEntityNotPresent(EntityType<?> entityType) {
        if (!entitiesIn(entityType, getBounds()).isEmpty()) {
            throw assertionException("test.error.unexpected_entity", entityType.getDescription());
        }
    }

    public void assertEntityNotPresent(EntityType<?> entityType, int x, int y, int z) {
        assertEntityNotPresent(entityType, new BlockPos(x, y, z));
    }

    public void assertEntityNotPresent(EntityType<?> entityType, BlockPos pos) {
        if (!entitiesIn(entityType, new AABB(absolutePos(pos))).isEmpty()) {
            throw assertionException(pos, "test.error.unexpected_entity", entityType.getDescription());
        }
    }

    public void assertEntityNotPresent(EntityType<?> entityType, AABB relativeAABB) {
        if (!entitiesIn(entityType, absoluteAABB(relativeAABB)).isEmpty()) {
            throw assertionException("test.error.unexpected_entity", entityType.getDescription());
        }
    }

    public void assertEntityTouching(EntityType<?> entityType, double x, double y, double z) {
        Vec3 point = new Vec3(x, y, z);
        AABB probe = new AABB(absoluteVec(point), absoluteVec(point)).inflate(1.0E-6);
        if (entitiesIn(entityType, probe.inflate(1.0)).stream().noneMatch(e -> e.getBoundingBox().intersects(probe))) {
            throw assertionException(BlockPos.containing(point), "test.error.expected_entity_touching", entityType.getDescription(), x, y, z);
        }
    }

    public void assertEntityNotTouching(EntityType<?> entityType, double x, double y, double z) {
        Vec3 point = new Vec3(x, y, z);
        AABB probe = new AABB(absoluteVec(point), absoluteVec(point)).inflate(1.0E-6);
        if (entitiesIn(entityType, probe.inflate(1.0)).stream().anyMatch(e -> e.getBoundingBox().intersects(probe))) {
            throw assertionException(BlockPos.containing(point), "test.error.unexpected_entity_touching", entityType.getDescription(), x, y, z);
        }
    }

    public <E extends Entity, T> void assertEntityData(BlockPos pos, EntityType<E> entityType, Predicate<E> test) {
        List<E> found = getEntities(entityType, pos, 0.5);
        if (found.isEmpty()) {
            throw assertionException(pos, "test.error.expected_entity", entityType.getDescription());
        }
        for (E entity : found) {
            if (!test.test(entity)) {
                throw assertionException(pos, "test.error.expected_entity_data_predicate", entityType.getDescription());
            }
        }
    }

    public <E extends Entity, T> void assertEntityData(BlockPos pos, EntityType<E> entityType, Function<? super E, T> dataAccessor, T data) {
        assertEntityData(pos, entityType, entity -> Objects.equals(dataAccessor.apply(entity), data));
    }

    public <E extends Entity, T> void assertEntityData(AABB box, EntityType<E> entityType, Function<? super E, T> dataAccessor, T data) {
        List<E> found = entitiesIn(entityType, absoluteAABB(box));
        if (found.isEmpty()) {
            throw assertionException("test.error.expected_entity_in_box", entityType.getDescription());
        }
        for (E entity : found) {
            if (!Objects.equals(dataAccessor.apply(entity), data)) {
                throw assertionException("test.error.expected_entity_data", data, dataAccessor.apply(entity));
            }
        }
    }

    public <E extends LivingEntity> void assertEntityIsHolding(BlockPos pos, EntityType<E> entityType, Item item) {
        assertEntityData(pos, entityType, entity -> entity.isHolding(item));
    }

    public <E extends Entity & net.minecraft.world.entity.npc.InventoryCarrier> void assertEntityInventoryContains(BlockPos pos, EntityType<E> entityType, Item item) {
        assertEntityData(pos, entityType, entity -> entity.getInventory().hasAnyMatching(stack -> stack.is(item)));
    }

    public void assertContainerEmpty(BlockPos pos) {
        if (!(getLevel().getBlockEntity(absolutePos(pos)) instanceof net.minecraft.world.Container container)) {
            throw assertionException(pos, "test.error.expected_container");
        }
        if (!container.isEmpty()) {
            throw assertionException(pos, "test.error.expected_empty_container");
        }
    }

    private net.minecraft.world.Container container(BlockPos pos) {
        if (getLevel().getBlockEntity(absolutePos(pos)) instanceof net.minecraft.world.Container container) {
            return container;
        }
        throw assertionException(pos, "test.error.expected_container");
    }

    public void assertContainerContainsSingle(BlockPos pos, Item item) {
        net.minecraft.world.Container container = container(pos);
        int stacks = 0;
        boolean match = false;
        for (int i = 0; i < container.getContainerSize(); i++) {
            ItemStack stack = container.getItem(i);
            if (!stack.isEmpty()) {
                stacks++;
                match = stack.is(item);
            }
        }
        if (stacks != 1 || !match) {
            throw assertionException(pos, "test.error.expected_container_contents_single", item.getDescriptionId());
        }
    }

    public void assertContainerContains(BlockPos pos, Item item) {
        net.minecraft.world.Container container = container(pos);
        for (int i = 0; i < container.getContainerSize(); i++) {
            if (container.getItem(i).is(item)) {
                return;
            }
        }
        throw assertionException(pos, "test.error.expected_container_contents", item.getDescriptionId());
    }

    public void assertSameBlockStates(BoundingBox sourceBoundingBox, BlockPos targetBoundingBoxCorner) {
        BlockPos.betweenClosedStream(sourceBoundingBox).forEach(source -> {
            BlockPos target = targetBoundingBoxCorner.offset(source.getX() - sourceBoundingBox.minX(), source.getY() - sourceBoundingBox.minY(), source.getZ() - sourceBoundingBox.minZ());
            assertSameBlockState(source, target);
        });
    }

    public void assertSameBlockState(BlockPos sourcePos, BlockPos targetPos) {
        BlockState source = getBlockState(sourcePos);
        BlockState target = getBlockState(targetPos);
        if (source != target) {
            throw assertionException(sourcePos, "test.error.state_not_equal", target, source);
        }
    }

    public void assertAtTickTimeContainerContains(long time, BlockPos pos, Item item) {
        runAtTickTime(time, () -> assertContainerContains(pos, item));
    }

    public void assertAtTickTimeContainerEmpty(long time, BlockPos pos) {
        runAtTickTime(time, () -> assertContainerEmpty(pos));
    }

    public <E extends Entity, T> void succeedWhenEntityData(BlockPos pos, EntityType<E> entityType, Function<E, T> dataAccessor, T data) {
        succeedWhen(() -> assertEntityData(pos, entityType, dataAccessor, data));
    }

    public <E extends Entity> void assertEntityProperty(E entity, Predicate<E> test, Component description) {
        if (!test.test(entity)) {
            throw assertionException(entity.blockPosition(), "test.error.entity_property", entity.getType().getDescription(), description);
        }
    }

    public <E extends Entity, T> void assertEntityProperty(E entity, Function<E, T> test, T expected, Component description) {
        T actual = test.apply(entity);
        if (!Objects.equals(actual, expected)) {
            throw assertionException(entity.blockPosition(), "test.error.entity_property_details", entity.getType().getDescription(), description, actual, expected);
        }
    }

    public void assertLivingEntityHasMobEffect(LivingEntity entity, Holder<MobEffect> mobEffect, int amplifier) {
        var instance = entity.getEffect(mobEffect);
        if (instance == null || instance.getAmplifier() != amplifier) {
            throw assertionException("test.error.expected_entity_effect", entity.getName(), mobEffect.getRegisteredName(), amplifier);
        }
    }

    public void succeedWhenEntityPresent(EntityType<?> entityType, int x, int y, int z) {
        succeedWhenEntityPresent(entityType, new BlockPos(x, y, z));
    }

    public void succeedWhenEntityPresent(EntityType<?> entityType, BlockPos pos) {
        succeedWhen(() -> assertEntityPresent(entityType, pos));
    }

    public void succeedWhenEntityNotPresent(EntityType<?> entityType, int x, int y, int z) {
        succeedWhenEntityNotPresent(entityType, new BlockPos(x, y, z));
    }

    public void succeedWhenEntityNotPresent(EntityType<?> entityType, BlockPos pos) {
        succeedWhen(() -> assertEntityNotPresent(entityType, pos));
    }
}
