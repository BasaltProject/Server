// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity;

import net.minecraft.nbt.CompoundTag;
import net.minecraft.resources.Identifier;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.world.flag.FeatureElement;
import net.minecraft.world.level.Level;
import net.minecraft.world.level.entity.EntityTypeTest;
import org.basaltmc.bukkit.EntityBridge;
import com.mojang.serialization.Codec;
import java.util.Optional;
import java.util.stream.Stream;
import net.minecraft.core.BlockPos;
import net.minecraft.core.Holder;
import net.minecraft.network.RegistryFriendlyByteBuf;
import net.minecraft.network.chat.Component;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.resources.ResourceKey;
import net.minecraft.tags.TagKey;
import net.minecraft.world.flag.FeatureFlag;
import net.minecraft.world.flag.FeatureFlagSet;
import net.minecraft.world.item.ItemStack;
import net.minecraft.world.item.component.TypedEntityData;
import net.minecraft.world.level.LevelReader;
import net.minecraft.world.level.block.Block;
import net.minecraft.world.level.block.state.BlockState;
import net.minecraft.world.level.storage.ValueInput;
import net.minecraft.world.level.storage.loot.LootTable;
import net.minecraft.world.phys.AABB;
import net.minecraft.world.phys.Vec3;

public class EntityType<T extends Entity> implements EntityTypeTest<Entity, T>, FeatureElement {
    private Identifier key;
    private EntityFactory<T> factory;
    private MobCategory customCategory;
    private boolean serialize = true;
    private boolean summon = true;
    private boolean customFireImmune;
    private boolean spawnFar;
    private TagKey<Block> immuneTo;
    private EntityDimensions customDimensions;
    private float spawnDimensionsScale = 1.0F;
    private int trackingRange = 5;
    private int interval = 3;
    private String descriptionId;
    private Optional<ResourceKey<LootTable>> lootTable = Optional.empty();
    private FeatureFlagSet features = FeatureFlagSet.of();
    private boolean allowedInPeaceful = true;
    private boolean customTrackDeltas = true;

    private org.basaltmc.bukkit.facade.EntityTypeData.Row row() {
        return key == null ? null : org.basaltmc.bukkit.facade.EntityTypeData.of(key.getPath());
    }

    private boolean flag(char flag) {
        org.basaltmc.bukkit.facade.EntityTypeData.Row row = row();
        return row != null && row.flags().indexOf(flag) >= 0;
    }

    public EntityType(Identifier key) {
        this.key = key;
    }

    public Entity create(Level level, EntitySpawnReason reason) {
        if (factory != null) {
            return factory.create((EntityType<T>) this, level);
        }
        Entity species = org.basaltmc.bukkit.facade.Species.create(this, level, false);
        if (species != null) {
            return species;
        }
        Entity made = EntityBridge.isAlive(key.toString())
            ? new LivingEntity((EntityType<?>) this, (ServerLevel) level)
            : new Entity((EntityType<?>) this, (ServerLevel) level);
        return made;
    }

    public static Identifier getKey(EntityType type) {
        return type.key;
    }

    public static Entity loadEntityRecursive(EntityType type, CompoundTag tag, Level level, EntitySpawnReason reason, EntityProcessor postLoad) {
        Entity made = type.create(level, reason);
        made.loadTag(tag);
        return postLoad == null ? made : postLoad.process(made);
    }
    public static final Codec<EntityType<?>> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.codec(net.minecraft.world.entity.EntityType.class, "CODEC");
    public static final StreamCodec<RegistryFriendlyByteBuf, EntityType<?>> STREAM_CODEC = (StreamCodec<RegistryFriendlyByteBuf, EntityType<?>>) (StreamCodec) net.minecraft.network.codec.ByteBufCodecs.registry(net.minecraft.core.registries.Registries.ENTITY_TYPE);
    public static final int NO_UPDATE_INTERVAL = 2147483647;
    public EntityType(EntityFactory<T> factory, MobCategory category, boolean serialize, boolean summon, boolean fireImmune, boolean canSpawnFarFromPlayer, TagKey<Block> immuneTo, EntityDimensions dimensions, float spawnDimensionsScale, int clientTrackingRange, int updateInterval, String descriptionId, Optional<ResourceKey<LootTable>> lootTable, FeatureFlagSet requiredFeatures, boolean allowedInPeaceful, boolean trackDeltas) {
        this.key = null;
        this.factory = factory;
        this.customCategory = category;
        this.serialize = serialize;
        this.summon = summon;
        this.customFireImmune = fireImmune;
        this.spawnFar = canSpawnFarFromPlayer;
        this.immuneTo = immuneTo;
        this.customDimensions = dimensions;
        this.spawnDimensionsScale = spawnDimensionsScale;
        this.trackingRange = clientTrackingRange;
        this.interval = updateInterval;
        this.descriptionId = descriptionId;
        this.lootTable = lootTable;
        this.features = requiredFeatures;
        this.allowedInPeaceful = allowedInPeaceful;
        this.customTrackDeltas = trackDeltas;
    }
    public T spawn(ServerLevel level, ItemStack itemStack, LivingEntity user, BlockPos spawnPos, EntitySpawnReason spawnReason, boolean tryMoveDown, boolean movedUp) {
        return spawn(level, (PostSpawnProcessor<T>) null, spawnPos, spawnReason, tryMoveDown, movedUp);
    }
    public static <T extends Entity> PostSpawnProcessor<T> createDefaultStackConfig(Level level, ItemStack itemStack, LivingEntity user) {
        return appendDefaultStackConfig(PostSpawnProcessor.nop(), level, itemStack, user);
    }
    public static <T extends Entity> PostSpawnProcessor<T> appendDefaultStackConfig(PostSpawnProcessor<T> initialConfig, Level level, ItemStack itemStack, LivingEntity user) {
        return appendCustomEntityStackConfig(appendComponentsConfig(initialConfig, itemStack), level, itemStack, user);
    }
    public static <T extends Entity> PostSpawnProcessor<T> appendComponentsConfig(PostSpawnProcessor<T> initialConfig, ItemStack itemStack) {
        return entity -> {
            initialConfig.apply(entity);
            entity.applyComponentsFromItemStack(itemStack);
        };
    }
    public static <T extends Entity> PostSpawnProcessor<T> appendCustomEntityStackConfig(PostSpawnProcessor<T> initialConfig, Level level, ItemStack itemStack, LivingEntity user) {
        return initialConfig;
    }
    public T spawn(ServerLevel level, BlockPos spawnPos, EntitySpawnReason spawnReason) {
        return spawn(level, (PostSpawnProcessor<T>) null, spawnPos, spawnReason, false, false);
    }
    public T spawn(ServerLevel level, PostSpawnProcessor<T> postSpawnConfig, BlockPos spawnPos, EntitySpawnReason spawnReason, boolean tryMoveDown, boolean movedUp) {
        T entity = create(level, postSpawnConfig, spawnPos, spawnReason, tryMoveDown, movedUp);
        if (entity != null) {
            level.addFreshEntityWithPassengers(entity, org.bukkit.event.entity.CreatureSpawnEvent.SpawnReason.DEFAULT);
        }
        return entity;
    }
    public T create(ServerLevel level, PostSpawnProcessor<T> postSpawnConfig, BlockPos spawnPos, EntitySpawnReason spawnReason, boolean tryMoveDown, boolean movedUp) {
        @SuppressWarnings("unchecked")
        T entity = (T) create(level, spawnReason);
        if (entity == null) {
            return null;
        }
        double y = spawnPos.getY();
        if (tryMoveDown) {
            y = spawnPos.getY() + getYOffset(level, spawnPos, movedUp, entity.getBoundingBox());
        }
        entity.absSnapTo(spawnPos.getX() + 0.5, y, spawnPos.getZ() + 0.5, net.minecraft.util.Mth.wrapDegrees(level.getRandom().nextFloat() * 360.0F), 0.0F);
        if (postSpawnConfig != null) {
            postSpawnConfig.apply(entity);
        }
        return entity;
    }
    protected static double getYOffset(LevelReader level, BlockPos spawnPos, boolean movedUp, AABB entityBox) {
        AABB box = entityBox.move(spawnPos.getX(), spawnPos.getY(), spawnPos.getZ());
        if (movedUp) {
            box = box.expandTowards(0.0, -1.0, 0.0);
        }
        Iterable<net.minecraft.world.phys.shapes.VoxelShape> floor = level.getCollisions(null, box);
        double top = spawnPos.getY();
        for (net.minecraft.world.phys.shapes.VoxelShape shape : floor) {
            for (AABB part : shape.toAabbs()) {
                top = Math.max(top, part.maxY);
            }
        }
        return top - spawnPos.getY();
    }
    public static void updateCustomEntityTag(Level level, LivingEntity user, Entity entity, TypedEntityData<EntityType<?>> entityData) {
        return;
    }
    public boolean canSerialize() {
        return factory != null ? serialize : !flag('s') && row() != null || row() == null && !key.getPath().equals("player");
    }
    public boolean canSummon() {
        return factory != null ? summon : !flag('u') && row() != null || row() == null && !key.getPath().equals("player");
    }
    public boolean fireImmune() {
        return factory != null ? customFireImmune : flag('f');
    }
    public boolean canSpawnFarFromPlayer() {
        return factory != null ? spawnFar : flag('c') || getCategory() == MobCategory.CREATURE;
    }
    public MobCategory getCategory() {
        if (customCategory != null) {
            return customCategory;
        }
        org.basaltmc.bukkit.facade.EntityTypeData.Row data = row();
        if (data != null) {
            return MobCategory.valueOf(data.category());
        }
        Class<?> kind = bukkitClass();
        if (kind == null) {
            return MobCategory.MISC;
        }
        if (org.bukkit.entity.Monster.class.isAssignableFrom(kind) || org.bukkit.entity.Slime.class.isAssignableFrom(kind)
                || org.bukkit.entity.Ghast.class.isAssignableFrom(kind) || org.bukkit.entity.Phantom.class.isAssignableFrom(kind)) {
            return MobCategory.MONSTER;
        }
        if (org.bukkit.entity.WaterMob.class.isAssignableFrom(kind)) {
            return org.bukkit.entity.Squid.class.isAssignableFrom(kind) || org.bukkit.entity.Dolphin.class.isAssignableFrom(kind)
                ? MobCategory.WATER_CREATURE : MobCategory.WATER_AMBIENT;
        }
        if (org.bukkit.entity.Ambient.class.isAssignableFrom(kind)) {
            return MobCategory.AMBIENT;
        }
        if (org.bukkit.entity.Animals.class.isAssignableFrom(kind)) {
            return MobCategory.CREATURE;
        }
        return MobCategory.MISC;
    }
    public String getDescriptionId() {
        if (descriptionId != null) {
            return descriptionId;
        }
        return "entity." + key.getNamespace() + "." + key.getPath().replace('/', '.');
    }
    public Component getDescription() {
        return net.minecraft.network.chat.Component.translatable(getDescriptionId());
    }
    public String toString() {
        return "entity." + key.getNamespace() + "." + key.getPath().replace('/', '.');
    }
    public String toShortString() {
        return key.getPath();
    }
    public Optional<ResourceKey<LootTable>> getDefaultLootTable() {
        if (factory != null) {
            return lootTable;
        }
        if (flag('l') || row() == null) {
            return Optional.empty();
        }
        return Optional.of((ResourceKey<LootTable>) (ResourceKey<?>) ResourceKey.create((ResourceKey) net.minecraft.core.registries.Registries.LOOT_TABLE,
            Identifier.fromNamespaceAndPath(key.getNamespace(), "entities/" + key.getPath())));
    }
    public float getWidth() {
        return getDimensions().width();
    }
    public float getHeight() {
        return getDimensions().height();
    }
    public FeatureFlagSet requiredFeatures() {
        return features;
    }
    public boolean canSpawn(Level level) {
        return true;
    }
    public T create(Level level, EntitySpawnRequest request) {
        T made = (T) create(level, request.reason());
        return made;
    }
    public static Optional<Entity> create(ValueInput input, Level level, EntitySpawnRequest request) {
        Optional<EntityType<?>> type = by(input);
        if (type.isEmpty()) {
            return Optional.empty();
        }
        return create(type.get(), input, level, request.reason());
    }
    public static Optional<Entity> create(EntityType<?> type, ValueInput input, Level level, EntitySpawnReason reason) {
        Entity made = type.create(level, reason);
        if (made == null) {
            return Optional.empty();
        }
        made.load(input);
        return Optional.of(made);
    }
    public AABB getSpawnAABB(Vec3 at) {
        return getSpawnAABB(at.x, at.y, at.z);
    }
    public AABB getSpawnAABB(double x, double y, double z) {
        EntityDimensions size = getDimensions();
        float half = size.width() / 2.0F;
        return new AABB(x - half, y, z - half, x + half, y + size.height(), z + half);
    }
    public boolean isBlockDangerous(BlockState state) {
        if (immuneTo != null && state.is(immuneTo)) {
            return false;
        }
        return !fireImmune() && (state.is(net.minecraft.tags.BlockTags.FIRE) || state.is(net.minecraft.world.level.block.Blocks.MAGMA_BLOCK)
            || state.is(net.minecraft.world.level.block.Blocks.LAVA)) || state.is(net.minecraft.world.level.block.Blocks.WITHER_ROSE);
    }
    public EntityDimensions getDimensions() {
        if (customDimensions != null) {
            return customDimensions;
        }
        org.basaltmc.bukkit.facade.EntityTypeData.Row data = row();
        return data == null ? EntityDimensions.scalable(0.6F, 1.8F) : EntityDimensions.scalable(data.width(), data.height()).withEyeHeight(data.eyeHeight());
    }
    public static Optional<EntityType<?>> by(ValueInput input) {
        return input.getString("id").map(Identifier::tryParse).flatMap(id -> net.minecraft.core.registries.BuiltInRegistries.ENTITY_TYPE.getOptional(id));
    }
    public static Entity loadEntityRecursive(CompoundTag tag, Level level, EntitySpawnRequest request, EntityProcessor postLoad) {
        return loadEntityRecursive(net.minecraft.world.level.storage.TagValueInput.createGlobal(net.minecraft.util.ProblemReporter.DISCARDING, tag), level, request.reason(), postLoad);
    }
    public static Entity loadEntityRecursive(ValueInput input, Level level, EntitySpawnReason reason, EntityProcessor postLoad) {
        Optional<EntityType<?>> type = by(input);
        return type.isEmpty() ? null : loadEntityRecursive(type.get(), input, level, reason, postLoad);
    }
    public static Entity loadEntityRecursive(ValueInput input, Level level, EntitySpawnRequest request, EntityProcessor postLoad) {
        return loadEntityRecursive(input, level, request.reason(), postLoad);
    }
    public static Entity loadEntityRecursive(EntityType<?> type, ValueInput input, Level level, EntitySpawnReason reason, EntityProcessor postLoad) {
        Entity made = type.create(level, reason);
        if (made == null) {
            return null;
        }
        made.load(input);
        made = postLoad == null ? made : postLoad.process(made);
        if (made == null) {
            return null;
        }
        for (ValueInput passengerInput : input.childrenListOrEmpty(Entity.TAG_PASSENGERS)) {
            Entity passenger = loadEntityRecursive(passengerInput, level, reason, postLoad);
            if (passenger != null) {
                passenger.startRiding(made, true, false);
            }
        }
        return made;
    }
    public static Stream<Entity> loadEntitiesRecursive(ValueInput.ValueInputList entities, Level level, EntitySpawnReason reason) {
        return entities.stream().map(input -> loadEntityRecursive(input, level, reason, EntityProcessor.NOP)).filter(java.util.Objects::nonNull);
    }
    public int clientTrackingRange() {
        org.basaltmc.bukkit.facade.EntityTypeData.Row data = row();
        return factory != null || data == null ? trackingRange : data.trackingRange();
    }
    public int updateInterval() {
        org.basaltmc.bukkit.facade.EntityTypeData.Row data = row();
        return factory != null || data == null ? interval : data.updateInterval();
    }
    public boolean hasUpdateInterval() {
        return updateInterval() != NO_UPDATE_INTERVAL;
    }
    public boolean trackDeltas() {
        return factory != null ? customTrackDeltas : !flag('d');
    }
    @SuppressWarnings("unchecked")
    public T tryCast(Entity entity) {
        return entity != null && EntityType.getKey(entity.getType()).equals(key) ? (T) entity : null;
    }
    public Class<? extends Entity> getBaseClass() {
        return Entity.class;
    }
    public Holder.Reference<EntityType<?>> builtInRegistryHolder() {
        return net.minecraft.core.registries.BuiltInRegistries.ENTITY_TYPE.getOrThrow(ResourceKey.create(net.minecraft.core.registries.Registries.ENTITY_TYPE, key));
    }
    public boolean isAllowedInPeaceful() {
        return factory != null ? allowedInPeaceful : !flag('p');
    }
    public boolean onlyOpCanSetNbt() {
        return false;
    }
    public static class Builder<T extends Entity> {
        private final EntityFactory<T> factory;
        private final MobCategory category;
        private boolean serialize = true;
        private boolean summon = true;
        private boolean fireImmune;
        private boolean spawnFar;
        private TagKey<Block> immuneTo;
        private float width = 0.6F;
        private float height = 1.8F;
        private float eyeHeight = Float.NaN;
        private float spawnScale = 1.0F;
        private int trackingRange = 5;
        private int updateInterval = 3;
        private FeatureFlagSet features = FeatureFlagSet.of();
        private boolean lootTable = true;
        private boolean inPeaceful = true;
        private boolean trackDeltas = true;
        private final EntityAttachments.Builder attachments = EntityAttachments.builder();

        private Builder(EntityFactory<T> factory, MobCategory category) {
            this.factory = factory;
            this.category = category;
            this.spawnFar = category == MobCategory.CREATURE || category == MobCategory.MISC;
        }
        public static <T extends Entity> Builder<T> of(EntityFactory<T> factory, MobCategory category) {
            return new Builder<>(factory, category);
        }
        public static <T extends Entity> Builder<T> createNothing(MobCategory category) {
            return new Builder<>((type, level) -> null, category);
        }
        public Builder<T> sized(float width, float height) {
            this.width = width;
            this.height = height;
            return this;
        }
        public Builder<T> spawnDimensionsScale(float scale) {
            this.spawnScale = scale;
            return this;
        }
        public Builder<T> eyeHeight(float eyeHeight) {
            this.eyeHeight = eyeHeight;
            return this;
        }
        public Builder<T> passengerAttachments(float... offsetYs) {
            for (float y : offsetYs) {
                attachments.attach(EntityAttachment.PASSENGER, 0.0F, y, 0.0F);
            }
            return this;
        }
        public Builder<T> passengerAttachments(Vec3... points) {
            for (Vec3 point : points) {
                attachments.attach(EntityAttachment.PASSENGER, point);
            }
            return this;
        }
        public Builder<T> vehicleAttachment(Vec3 point) {
            return attach(EntityAttachment.VEHICLE, point);
        }
        public Builder<T> ridingOffset(float ridingOffset) {
            return attach(EntityAttachment.VEHICLE, 0.0F, -ridingOffset, 0.0F);
        }
        public Builder<T> nameTagOffset(float nameTagOffset) {
            return attach(EntityAttachment.NAME_TAG, 0.0F, nameTagOffset, 0.0F);
        }
        public Builder<T> attach(EntityAttachment attachment, float x, float y, float z) {
            attachments.attach(attachment, x, y, z);
            return this;
        }
        public Builder<T> attach(EntityAttachment attachment, Vec3 point) {
            attachments.attach(attachment, point);
            return this;
        }
        public Builder<T> noSummon() {
            summon = false;
            return this;
        }
        public Builder<T> noSave() {
            serialize = false;
            return this;
        }
        public Builder<T> fireImmune() {
            fireImmune = true;
            return this;
        }
        public Builder<T> immuneTo(TagKey<Block> tag) {
            immuneTo = tag;
            return this;
        }
        public Builder<T> canSpawnFarFromPlayer() {
            spawnFar = true;
            return this;
        }
        public Builder<T> clientTrackingRange(int clientChunkRange) {
            trackingRange = clientChunkRange;
            return this;
        }
        public Builder<T> updateInterval(int updateInterval) {
            this.updateInterval = updateInterval;
            return this;
        }
        public Builder<T> noUpdateInterval() {
            return updateInterval(NO_UPDATE_INTERVAL);
        }
        public Builder<T> requiredFeatures(FeatureFlag... flags) {
            features = flags.length == 0 ? FeatureFlagSet.of() : flags.length == 1 ? FeatureFlagSet.of(flags[0])
                : FeatureFlagSet.of(flags[0], java.util.Arrays.copyOfRange(flags, 1, flags.length));
            return this;
        }
        public Builder<T> noLootTable() {
            lootTable = false;
            return this;
        }
        public Builder<T> notInPeaceful() {
            inPeaceful = false;
            return this;
        }
        public Builder<T> dontTrackDeltas() {
            trackDeltas = false;
            return this;
        }
        public EntityType<T> build(ResourceKey<EntityType<?>> name) {
            EntityDimensions size = EntityDimensions.scalable(width, height).withAttachments(attachments);
            if (!Float.isNaN(eyeHeight)) {
                size = size.withEyeHeight(eyeHeight);
            }
            Identifier id = name.identifier();
            Optional<ResourceKey<LootTable>> table = lootTable ? Optional.of((ResourceKey<LootTable>) (ResourceKey<?>) ResourceKey.create(
                (ResourceKey) net.minecraft.core.registries.Registries.LOOT_TABLE,
                Identifier.fromNamespaceAndPath(id.getNamespace(), "entities/" + id.getPath()))) : Optional.empty();
            EntityType<T> built = new EntityType<>(factory, category, serialize, summon, fireImmune, spawnFar, immuneTo, size, spawnScale,
                trackingRange, updateInterval, "entity." + id.getNamespace() + "." + id.getPath().replace('/', '.'), table, features, inPeaceful, trackDeltas);
            built.key = id;
            return built;
        }
    }
    public interface EntityFactory<T extends Entity> {
        T create(EntityType<T> entityType, Level level);
    }
    private Class<?> bukkitClass() {
        org.bukkit.entity.EntityType type = org.bukkit.Registry.ENTITY_TYPE.get(org.bukkit.NamespacedKey.fromString(key.toString()));
        return type == null ? null : type.getEntityClass();
    }
}
