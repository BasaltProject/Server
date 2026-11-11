// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity;

import java.util.UUID;
import net.minecraft.core.RegistryAccess;
import net.minecraft.core.TypedInstance;
import net.minecraft.core.component.DataComponentGetter;
import net.minecraft.nbt.CompoundTag;
import net.minecraft.nbt.DoubleTag;
import net.minecraft.nbt.FloatTag;
import net.minecraft.nbt.ListTag;
import net.minecraft.nbt.Tag;
import net.minecraft.nbt.StringTag;
import net.minecraft.util.Mth;
import net.minecraft.world.level.ClipContext;
import net.minecraft.network.syncher.SyncedDataHolder;
import net.minecraft.resources.Identifier;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.util.debug.DebugValueSource;
import net.minecraft.world.Nameable;
import net.minecraft.world.level.entity.EntityAccess;
import net.minecraft.world.level.storage.ValueInput;
import net.minecraft.world.level.storage.ValueOutput;
import net.minecraft.world.phys.Vec3;
import net.minecraft.world.scores.ScoreHolder;
import org.basaltmc.bukkit.EntityBridge;
import org.basaltmc.bukkit.Unsupported;
import org.basaltmc.bukkit.WorldNatives;
import org.basaltmc.bukkit.facade.NbtCodec;
import org.basaltmc.bukkit.facade.ServerAccess;
import org.basaltmc.bukkit.facade.ValueIo;
import org.bukkit.craftbukkit.entity.CraftEntity;
import org.bukkit.event.entity.CreatureSpawnEvent;
import java.util.List;
import java.util.Optional;
import java.util.Set;
import java.util.function.BiConsumer;
import java.util.function.Predicate;
import java.util.stream.Stream;
import net.minecraft.CrashReportCategory;
import net.minecraft.commands.CommandSourceStack;
import net.minecraft.commands.arguments.EntityAnchorArgument;
import net.minecraft.core.BlockPos;
import net.minecraft.core.Direction;
import net.minecraft.core.Holder;
import net.minecraft.core.PositionAndRotation;
import net.minecraft.core.component.DataComponentType;
import net.minecraft.network.chat.Component;
import net.minecraft.network.chat.HoverEvent;
import net.minecraft.network.protocol.Packet;
import net.minecraft.network.protocol.game.ClientGamePacketListener;
import net.minecraft.network.protocol.game.ClientboundAddEntityPacket;
import net.minecraft.network.protocol.game.VecDeltaCodec;
import net.minecraft.network.syncher.EntityDataAccessor;
import net.minecraft.network.syncher.SynchedEntityData;
import net.minecraft.resources.ResourceKey;
import net.minecraft.server.level.ServerEntity;
import net.minecraft.server.level.ServerPlayer;
import net.minecraft.sounds.SoundEvent;
import net.minecraft.sounds.SoundSource;
import net.minecraft.tags.TagKey;
import net.minecraft.util.BlockUtil;
import net.minecraft.util.ProblemReporter;
import net.minecraft.util.RandomSource;
import net.minecraft.world.InteractionHand;
import net.minecraft.world.InteractionResult;
import net.minecraft.world.damagesource.DamageSource;
import net.minecraft.world.damagesource.DamageSources;
import net.minecraft.world.entity.item.ItemEntity;
import net.minecraft.world.entity.player.Player;
import net.minecraft.world.entity.projectile.Projectile;
import net.minecraft.world.entity.projectile.ProjectileDeflection;
import net.minecraft.world.item.Item;
import net.minecraft.world.item.ItemStack;
import net.minecraft.world.level.BlockGetter;
import net.minecraft.world.level.ChunkPos;
import net.minecraft.world.level.Explosion;
import net.minecraft.world.level.ItemLike;
import net.minecraft.world.level.Level;
import net.minecraft.world.level.block.Mirror;
import net.minecraft.world.level.block.Portal;
import net.minecraft.world.level.block.Rotation;
import net.minecraft.world.level.block.state.BlockState;
import net.minecraft.world.level.entity.EntityInLevelCallback;
import net.minecraft.world.level.gameevent.DynamicGameEventListener;
import net.minecraft.world.level.gameevent.GameEvent;
import net.minecraft.world.level.material.Fluid;
import net.minecraft.world.level.material.FluidState;
import net.minecraft.world.level.material.PushReaction;
import net.minecraft.world.level.portal.TeleportTransition;
import net.minecraft.world.level.storage.loot.LootTable;
import net.minecraft.world.phys.AABB;
import net.minecraft.world.phys.HitResult;
import net.minecraft.world.phys.Vec2;
import net.minecraft.world.phys.shapes.CollisionContext;
import net.minecraft.world.phys.shapes.VoxelShape;
import net.minecraft.world.scores.PlayerTeam;
import net.minecraft.world.scores.Team;
import org.joml.Quaternionf;

public class Entity implements Nameable, EntityAccess, ScoreHolder, SyncedDataHolder, DataComponentGetter, ItemOwner, SlotProvider, DebugValueSource, TypedInstance<EntityType<?>> {
    public static final net.minecraft.network.syncher.EntityDataAccessor<Byte> DATA_SHARED_FLAGS_ID =
        net.minecraft.network.syncher.EntityDataSerializers.BYTE.createAccessor(0);
    private final net.minecraft.network.syncher.SynchedEntityData entityData = new net.minecraft.network.syncher.SynchedEntityData(this);
    private org.bukkit.entity.Entity boundEntity;
    private byte unboundFlags;
    public void bindBukkitEntity(org.bukkit.entity.Entity entity) { boundEntity=entity; }
    public net.minecraft.network.syncher.SynchedEntityData getEntityData(){return entityData;}
    public byte sharedFlags(){
        org.bukkit.entity.Entity entity=boundEntity;
        if(entity==null && isBound())entity=(org.bukkit.entity.Entity)getBukkitEntity();
        if(entity==null)return unboundFlags;
        int flags=(entity.getFireTicks()>0||entity.isVisualFire()?1:0)|(entity.isInvisible()?32:0)|(entity.isGlowing()?64:0);
        if(entity instanceof org.bukkit.entity.LivingEntity living){if(living.isSwimming())flags|=16;if(living.isGliding())flags|=128;}
        if(entity instanceof org.bukkit.entity.Player player){if(player.isSneaking())flags|=2;if(player.isSprinting())flags|=8;}
        return (byte)flags;
    }
    public void sharedFlags(byte flags){
        unboundFlags=flags;
        org.bukkit.entity.Entity entity=boundEntity;if(entity==null && isBound())entity=(org.bukkit.entity.Entity)getBukkitEntity();
        if(entity!=null){entity.setGlowing((flags&64)!=0);entity.setInvisible((flags&32)!=0);entity.setVisualFire((flags&1)!=0);
            if(entity instanceof org.bukkit.entity.Player player){player.setSneaking((flags&2)!=0);player.setSprinting((flags&8)!=0);}
            if(entity instanceof org.bukkit.entity.LivingEntity living){living.setSwimming((flags&16)!=0);living.setGliding((flags&128)!=0);}}
    }
    public boolean generation = false;
    public CreatureSpawnEvent.SpawnReason spawnReason;

    private ServerLevel level;
    private EntityType<?> type;
    private int id = -1;
    private UUID uuid;
    private CompoundTag pending;
    private CompoundTag snapshot;
    private boolean removed;

    protected Entity() {
    }

    public Entity(EntityType<?> type, ServerLevel level) {
        this.type = type;
        this.level = level;
        this.pending = new CompoundTag();
        this.pending.putString("id", EntityType.getKey(type).toString());
    }

    public void rebindAsView(int id, UUID uuid) {
        this.pending = null;
        this.id = id;
        this.uuid = uuid;
    }

    protected void onSpawned() {
    }

    public static Entity view(ServerLevel level, int id, UUID uuid, EntityType<?> type) {
        Entity species = org.basaltmc.bukkit.facade.Species.view(level, id, uuid, type);
        if (species != null) {
            return species;
        }
        Entity made = EntityBridge.isAlive(EntityType.getKey(type).toString()) ? new LivingEntity() : new Entity();
        made.level = level;
        made.id = id;
        made.uuid = uuid;
        made.type = type;
        return made;
    }


    private int invulnerableTime;

    public org.bukkit.entity.Entity live() {
        if (pending != null || removed || id < 0) {
            return null;
        }
        return boundEntity != null ? boundEntity : (org.bukkit.entity.Entity) getBukkitEntity();
    }

    private CompoundTag source() {
        return pending != null ? pending : snapshot;
    }

    private Tag pendingTag(String name) {
        CompoundTag tag = source();
        return tag == null ? null : tag.get(name);
    }

    private byte pendingByte(String name) {
        CompoundTag tag = source();
        return tag == null ? 0 : tag.getByte(name).orElse((byte) 0);
    }

    private double posComponent(int axis) {
        org.bukkit.entity.Entity live = live();
        if (live != null) {
            org.bukkit.Location at = live.getLocation();
            return axis == 0 ? at.getX() : axis == 1 ? at.getY() : at.getZ();
        }
        CompoundTag tag = source();
        return tag == null ? 0.0 : tag.getList("Pos").flatMap(list -> list.getDouble(axis)).orElse(0.0);
    }

    private float rotComponent(int axis) {
        org.bukkit.entity.Entity live = live();
        if (live != null) {
            org.bukkit.Location at = live.getLocation();
            return axis == 0 ? at.getYaw() : at.getPitch();
        }
        CompoundTag tag = source();
        return tag == null ? 0.0F : tag.getList("Rotation").flatMap(list -> list.getFloat(axis)).orElse(0.0F);
    }

    private Vec3 motion() {
        org.bukkit.entity.Entity live = live();
        if (live != null) {
            org.bukkit.util.Vector velocity = live.getVelocity();
            return new Vec3(velocity.getX(), velocity.getY(), velocity.getZ());
        }
        CompoundTag tag = source();
        if (tag == null) {
            return Vec3.ZERO;
        }
        return tag.getList("Motion").map(list -> new Vec3(list.getDouble(0).orElse(0.0), list.getDouble(1).orElse(0.0),
            list.getDouble(2).orElse(0.0))).orElse(Vec3.ZERO);
    }

    private void setMotion(double x, double y, double z) {
        org.bukkit.entity.Entity live = live();
        if (live != null) {
            live.setVelocity(new org.bukkit.util.Vector(x, y, z));
        } else if (pending != null) {
            ListTag list = new ListTag();
            list.add(DoubleTag.valueOf(x));
            list.add(DoubleTag.valueOf(y));
            list.add(DoubleTag.valueOf(z));
            pending.put("Motion", list);
        }
    }

    public static Entity fromBukkit(org.bukkit.entity.Entity other) {
        if (other instanceof org.bukkit.craftbukkit.entity.CraftEntity craft) {
            return craft.getHandle();
        }
        return EntityBridge.handle(other.getWorld(), other.getEntityId(), other.getUniqueId(), other.getType().key().asString());
    }

    protected Entity wrap(org.bukkit.entity.Entity other) {
        return EntityBridge.handle(other.getWorld(), other.getEntityId(), other.getUniqueId(), other.getType().key().asString());
    }

    public int getId() {
        return id;
    }

    public boolean isBound() {
        return id >= 0;
    }

    public ServerLevel level() {
        return level;
    }

    private String world() {
        if (level == null) {
            throw new IllegalStateException("Entity " + getClass().getSimpleName() + " is not attached to a level");
        }
        return level.getWorld().getName();
    }

    public void absSnapTo(double x, double y, double z, float yRot, float xRot) {
        if (pending == null) {
            WorldNatives.entityMove(world(), id, x, y, z, yRot, xRot);
            return;
        }
        ListTag pos = new ListTag();
        pos.add(DoubleTag.valueOf(x));
        pos.add(DoubleTag.valueOf(y));
        pos.add(DoubleTag.valueOf(z));
        pending.put("Pos", pos);
        ListTag rotation = new ListTag();
        rotation.add(FloatTag.valueOf(yRot));
        rotation.add(FloatTag.valueOf(xRot));
        pending.put("Rotation", rotation);
    }

    public void discard() {
        if (pending != null) {
            removed = true;
            return;
        }
        snapshot = nbt();
        removed = true;
        WorldNatives.entityRemove(world(), id);
    }

    public boolean isRemoved() {
        return removed || (pending == null && WorldNatives.entityStrings(world(), id) == null);
    }

    public CraftEntity getBukkitEntity() {
        if (pending != null) {
            throw Unsupported.hit("net.minecraft.world.entity.Entity.getBukkitEntity");
        }
        return (CraftEntity) EntityBridge.of(level.getWorld(), id);
    }

    public Vec3 getPosition(float partialTickTime) {
        return position();
    }

    public String getStringUUID() {
        return getUUID().toString();
    }

    public EntityType getType() {
        return type;
    }

    public UUID getUUID() {
        if (uuid == null) {
            int[] parts = pending == null ? null : pending.getIntArray("UUID").orElse(null);
            uuid = parts != null && parts.length == 4
                ? new UUID((long) parts[0] << 32 | (parts[1] & 0xFFFFFFFFL), (long) parts[2] << 32 | (parts[3] & 0xFFFFFFFFL))
                : UUID.randomUUID();
        }
        return uuid;
    }

    public void setUUID(UUID value) {
        uuid = value;
        if (pending != null) {
            pending.putIntArray("UUID", new int[] {(int) (value.getMostSignificantBits() >> 32),
                (int) value.getMostSignificantBits(), (int) (value.getLeastSignificantBits() >> 32),
                (int) value.getLeastSignificantBits()});
        }
    }

    public void load(ValueInput input) {
        loadTag(ValueIo.read(input));
    }

    public void loadTag(CompoundTag tag) {
        if (pending == null) {
            throw Unsupported.hit("net.minecraft.world.entity.Entity.load");
        }
        CompoundTag data = tag.copy();
        data.remove("id");
        pending.merge(data);
        uuid = null;
    }

    public RegistryAccess registryAccess() {
        return ServerAccess.registries();
    }

    public boolean save(ValueOutput output) {
        CompoundTag tag = nbt();
        if (tag == null) {
            return false;
        }
        ValueIo.write(output, tag);
        return true;
    }

    public void saveWithoutId(ValueOutput output) {
        CompoundTag tag = nbt();
        if (tag != null) {
            tag.remove("id");
            ValueIo.write(output, tag);
        }
    }

    public CompoundTag nbt() {
        if (pending != null) {
            return pending.copy();
        }
        if (!removed) {
            CompoundTag live = NbtCodec.fromBytes(WorldNatives.entityNbt(world(), id));
            if (live != null) {
                return live;
            }
        }
        return snapshot == null ? null : snapshot.copy();
    }

    public boolean spawnIn(String cause) {
        if (pending == null) {
            return true;
        }
        int made = WorldNatives.spawnEntityNbt(world(), NbtCodec.toBytes(pending), cause);
        if (made < 0) {
            return false;
        }
        id = made;
        snapshot = pending;
        pending = null;
        onSpawned();
        return true;
    }

    public static Identifier keyOf(Entity entity) {
        return EntityType.getKey(entity.type);
    }
    public static final String TAG_ID = "id";
    public static final String TAG_UUID = "UUID";
    public static final String TAG_PASSENGERS = "Passengers";
    public static final String TAG_DATA = "data";
    public static final String TAG_POS = "Pos";
    public static final String TAG_MOTION = "Motion";
    public static final String TAG_ROTATION = "Rotation";
    public static final String TAG_PORTAL_COOLDOWN = "PortalCooldown";
    public static final String TAG_NO_GRAVITY = "NoGravity";
    public static final String TAG_AIR = "Air";
    public static final String TAG_ON_GROUND = "OnGround";
    public static final String TAG_FALL_DISTANCE = "fall_distance";
    public static final String TAG_FIRE = "Fire";
    public static final String TAG_SILENT = "Silent";
    public static final String TAG_GLOWING = "Glowing";
    public static final String TAG_INVULNERABLE = "Invulnerable";
    public static final String TAG_INVULNERABLE_TIME = "invulnerable_time";
    public static final String TAG_CUSTOM_NAME = "CustomName";
    public static final int INVALID_ENTITY_ID = 0;
    public static final int CONTENTS_SLOT_INDEX = 0;
    public static final int BOARDING_COOLDOWN = 60;
    public static final int TOTAL_AIR_SUPPLY = 300;
    public static final int MAX_ENTITY_TAG_COUNT = 1024;
    public static final double DEFAULT_NAME_TAG_DISTANCE = 64.0;
    public static final double DEFAULT_BELOW_NAME_DISTANCE = 10.0;
    public static final double MAX_NAME_TAG_DISTANCE = 512.0;
    public static final float DELTA_AFFECTED_BY_BLOCKS_BELOW_0_2 = 0.2F;
    public static final double DELTA_AFFECTED_BY_BLOCKS_BELOW_0_5 = 0.500001;
    public static final double DELTA_AFFECTED_BY_BLOCKS_BELOW_1_0 = 0.999999;
    public static final int BASE_TICKS_REQUIRED_TO_FREEZE = 140;
    public static final int FREEZE_HURT_FREQUENCY = 40;
    public static final int BASE_SAFE_FALL_DISTANCE = 3;
    public boolean blocksBuilding;
    protected int boardingCooldown;
    public double xo;
    public double yo;
    public double zo;
    public float yRotO;
    public float xRotO;
    public boolean horizontalCollision;
    public boolean verticalCollision;
    public boolean verticalCollisionBelow;
    public boolean minorHorizontalCollision;
    protected Vec3 stuckSpeedMultiplier;
    public static final float DEFAULT_BB_WIDTH = 0.6F;
    public static final float DEFAULT_BB_HEIGHT = 1.8F;
    public float moveDist;
    public float flyDist;
    public double fallDistance;
    public double xOld;
    public double yOld;
    public double zOld;
    public boolean noPhysics;
    protected final RandomSource random = RandomSource.create();
    public int tickCount;
    protected boolean wasTouchingWater;
    protected boolean wasEyeInWater;
    protected boolean firstTick;
    protected static final int FLAG_ONFIRE = 0;
    protected static final int FLAG_GLOWING = 6;
    protected static final int FLAG_FALL_FLYING = 7;
    protected static final EntityDataAccessor<Pose> DATA_POSE = net.minecraft.network.syncher.EntityDataSerializers.POSE.createAccessor(6);
    public boolean needsSync;
    public boolean syncPosition;
    public boolean syncVelocity;
    public PortalProcessor portalProcess;
    protected String stringUUID;
    public boolean isInPowderSnow;
    public boolean wasInPowderSnow;
    public Optional<BlockPos> mainSupportingBlockPos;
    public static final int MAX_MOVEMENTS_HANDELED_PER_TICK = 100;
    protected final InsideBlockEffectApplier.StepBasedCollector insideEffectCollector = null;
    protected final InterpolationHandler interpolationHandler = null;
    public Entity(EntityType<?> type, Level level) {
        this(type, (ServerLevel) level);
    }
    public boolean isColliding(BlockPos pos, BlockState state) {
        AABB box = getBoundingBox();
        for (AABB part : state.getCollisionShape(level(), pos).move(pos.getX(), pos.getY(), pos.getZ()).toAabbs()) {
            if (part.intersects(box)) {
                return true;
            }
        }
        return false;
    }
    public int getTeamColor() {
        return 16777215;
    }
    public boolean isSpectator() {
        return false;
    }
    public boolean canInteractWithLevel() {
        return true;
    }
    public void unRide() {
        if (live() != null) {
            live().leaveVehicle();
        }
    }
    public void syncPacketPositionCodec(double x, double y, double z) {
    }
    public VecDeltaCodec getPositionCodec() {
        return positionCodec;
    }
    public Holder<EntityType<?>> typeHolder() {
        return getType().builtInRegistryHolder();
    }
    public boolean getRequiresPrecisePosition() {
        return false;
    }
    public void setRequiresPrecisePosition(boolean requiresPrecisePosition) {

    }
    public void setId(int id) {
        this.id = id;
    }
    public Set<String> entityTags() {
        org.bukkit.entity.Entity live = live();
        if (live != null) {
            return live.getScoreboardTags();
        }
        Set<String> tags = new java.util.LinkedHashSet<>();
        if (pending != null) {
            pending.getList("Tags").ifPresent(list -> list.forEach(tag -> tag.asString().ifPresent(tags::add)));
        }
        return tags;
    }
    public boolean addTag(String tag) {
        org.bukkit.entity.Entity live = live();
        if (live != null) {
            return live.addScoreboardTag(tag);
        }
        if (entityTags().contains(tag)) {
            return false;
        }
        ListTag list = pending.getList("Tags").orElseGet(ListTag::new);
        list.add(StringTag.valueOf(tag));
        pending.put("Tags", list);
        return true;
    }
    public boolean removeTag(String tag) {
        org.bukkit.entity.Entity live = live();
        if (live != null) {
            return live.removeScoreboardTag(tag);
        }
        if (pending == null || !entityTags().contains(tag)) {
            return false;
        }
        ListTag list = new ListTag();
        entityTags().stream().filter(kept -> !kept.equals(tag)).forEach(kept -> list.add(StringTag.valueOf(kept)));
        pending.put("Tags", list);
        return true;
    }
    public void kill(ServerLevel level) {
        remove(RemovalReason.KILLED);
    }
    protected void defineSynchedData(SynchedEntityData.Builder entityData) {
    }
    public boolean equals(Object obj) {
        return obj instanceof Entity other && other.getId() == getId() && (getId() >= 0 || other == this);
    }
    public int hashCode() {
        return getId();
    }
    public void remove(RemovalReason reason) {
        if (reason.shouldDestroy()) {
            discard();
        }
    }
    public void onClientRemoval() {

    }
    public void onRemoval(RemovalReason reason) {
    }
    public void setPose(Pose pose) {
        org.bukkit.entity.Entity live = live();
        if (live != null) {
            live.setPose(org.bukkit.entity.Pose.valueOf(pose == Pose.CROUCHING ? "SNEAKING" : pose.name()), true);
        }
    }
    public Pose getPose() {
        org.bukkit.entity.Entity live = live();
        if (live == null) {
            return Pose.STANDING;
        }
        String name = live.getPose().name();
        return Pose.valueOf(name.equals("SNEAKING") ? "CROUCHING" : name);
    }
    public boolean hasPose(Pose pose) {
        return getPose() == pose;
    }
    public boolean closerThan(Entity other, double distance) {
        return distanceToSqr(other) < distance * distance;
    }
    public boolean closerThan(Entity other, double distanceXZ, double distanceY) {
        double dx = other.getX() - getX();
        double dy = other.getY() - getY();
        double dz = other.getZ() - getZ();
        return Mth.lengthSquared(dx, dz) < Mth.square(distanceXZ) && Mth.square(dy) < Mth.square(distanceY);
    }
    protected void setRot(float yRot, float xRot) {
        absSnapRotationTo(yRot, xRot);
    }
    public void setPos(Vec3 pos) {
        setPos(pos.x, pos.y, pos.z);
    }
    public void setPos(double x, double y, double z) {
        absSnapTo(x, y, z, getYRot(), getXRot());
    }
    protected AABB makeBoundingBox() {
        return makeBoundingBox(position());
    }
    protected AABB makeBoundingBox(Vec3 position) {
        return getDimensions(getPose()).makeBoundingBox(position);
    }
    protected void reapplyPosition() {
    }
    public void turn(double xo, double yo) {
        setXRot(net.minecraft.util.Mth.clamp(getXRot() + (float) yo * 0.15F, -90.0F, 90.0F));
        setYRot(getYRot() + (float) xo * 0.15F);
    }
    public void updateDataBeforeSync() {

    }
    public void tick() {
        baseTick();
    }
    public void commonTick() {
    }
    public void baseTick() {
        tickCount++;
    }
    protected void computeSpeed() {
    }
    public void setSharedFlagOnFire(boolean value) {
        setSharedFlag(0, value);
    }
    public void checkBelowWorld() {
        if (getY() < level().getMinY() - 64) {
            onBelowWorld();
        }
    }
    public void setPortalCooldown() {
        return;
    }
    public void setPortalCooldown(int portalCooldown) {
        return;
    }
    public int getPortalCooldown() {
        return 0;
    }
    public boolean isOnPortalCooldown() {
        return false;
    }
    protected void processPortalCooldown() {
    }
    public void lavaIgnite() {
        if (!fireImmune() && live() != null) {
            live().setFireTicks(Math.max(live().getFireTicks(), 300));
        }
    }
    public void lavaHurt() {
        if (!fireImmune() && live() instanceof org.bukkit.entity.Damageable damageable) {
            damageable.damage(4.0);
        }
    }
    protected boolean shouldPlayLavaHurtSound() {
        return true;
    }
    public void igniteForSeconds(float numberOfSeconds) {
        setRemainingFireTicks(Mth.floor(numberOfSeconds * 20.0F));
    }
    public void igniteForTicks(int numberOfTicks) {
        setRemainingFireTicks(numberOfTicks);
    }
    public void setRemainingFireTicks(int remainingTicks) {
        org.bukkit.entity.Entity live = live();
        if (live != null) {
            live.setFireTicks(remainingTicks);
        } else if (pending != null) {
            pending.putShort("Fire", (short) remainingTicks);
        }
    }
    public int getRemainingFireTicks() {
        org.bukkit.entity.Entity live = live();
        return live != null ? live.getFireTicks() : pending == null ? 0 : pending.getShort("Fire").orElse((short) 0);
    }
    public void clearFire() {
        setRemainingFireTicks(0);
    }
    protected void onBelowWorld() {
        discard();
    }
    public boolean isFree(double xa, double ya, double za) {
        AABB box = getBoundingBox().move(xa, ya, za);
        return level().noCollision(this, box) && !level().containsAnyLiquid(box);
    }
    public void setOnGround(boolean onGround) {

    }
    public void setOnGroundWithMovement(boolean onGround, Vec3 movement) {
        setOnGround(onGround);
        verticalCollisionBelow = onGround && movement.y != 0.0;
    }
    public void setOnGroundWithMovement(boolean onGround, boolean horizontalCollision, Vec3 movement) {
        setOnGround(onGround);
        verticalCollisionBelow = onGround && movement.y != 0.0;
    }
    public boolean isSupportedBy(BlockPos pos) {
        return false;
    }
    protected void checkSupportingBlock(boolean onGround, Vec3 movement) {
    }
    public boolean onGround() {
        org.bukkit.entity.Entity live = live();
        if (live != null) {
            return live.isOnGround();
        }
        return pendingTag("OnGround") != null && pendingByte("OnGround") != 0;
    }
    public void move(MoverType moverType, Vec3 delta) {
        if (noPhysics) {
            absSnapTo(getX() + delta.x, getY() + delta.y, getZ() + delta.z, getYRot(), getXRot());
            return;
        }
        Vec3 movement = delta;
        if (moverType == MoverType.PISTON) {
            movement = limitPistonMovement(movement);
            if (movement.equals(Vec3.ZERO)) {
                return;
            }
        }
        Vec3 collided = collide(movement);
        double collidedLength = collided.lengthSqr();
        if (collidedLength > 1.0E-7 || movement.lengthSqr() - collidedLength < 1.0E-7) {
            absSnapTo(getX() + collided.x, getY() + collided.y, getZ() + collided.z, getYRot(), getXRot());
        }
        boolean xBlocked = !net.minecraft.util.Mth.equal(movement.x, collided.x);
        boolean zBlocked = !net.minecraft.util.Mth.equal(movement.z, collided.z);
        horizontalCollision = xBlocked || zBlocked;
        verticalCollision = movement.y != collided.y;
        verticalCollisionBelow = verticalCollision && movement.y < 0.0;
        minorHorizontalCollision = horizontalCollision && isHorizontalCollisionMinor(collided);
        setOnGroundWithMovement(verticalCollisionBelow, horizontalCollision, collided);
        Vec3 motion = getDeltaMovement();
        if (horizontalCollision) {
            setDeltaMovement(xBlocked ? 0.0 : motion.x, motion.y, zBlocked ? 0.0 : motion.z);
        }
        if (verticalCollision && motion.y != 0.0) {
            setDeltaMovement(getDeltaMovement().x, 0.0, getDeltaMovement().z);
        }
    }
    protected Vec3 collide(Vec3 movement) {
        AABB box = getBoundingBox();
        List<VoxelShape> entityColliders = level().getEntityCollisions(this, box.expandTowards(movement));
        Vec3 result = movement.lengthSqr() == 0.0 ? movement : collideBoundingBox(this, movement, box, level(), entityColliders);
        boolean xBlocked = movement.x != result.x;
        boolean yBlocked = movement.y != result.y;
        boolean zBlocked = movement.z != result.z;
        boolean landing = yBlocked && movement.y < 0.0;
        if (maxUpStep() > 0.0F && (landing || onGround()) && (xBlocked || zBlocked)) {
            AABB grounded = landing ? box.move(0.0, result.y, 0.0) : box;
            AABB stepBox = grounded.expandTowards(movement.x, maxUpStep(), movement.z);
            if (!landing) {
                stepBox = stepBox.expandTowards(0.0, -1.0E-5, 0.0);
            }
            List<VoxelShape> colliders = collectAllColliders(this, level(), stepBox);
            double[] stepped = org.basaltmc.bukkit.MechanicsNatives.stepUp(flatten(colliders), corners(grounded),
                grounded.minY - box.minY, movement.x, movement.y, movement.z, maxUpStep(), result.horizontalDistanceSqr());
            if (stepped != null) {
                return new Vec3(stepped[0], stepped[1], stepped[2]);
            }
        }
        return result;
    }
    private static Vec3 collideWithShapes(Vec3 movement, AABB box, List<VoxelShape> shapes) {
        if (shapes.isEmpty()) {
            return movement;
        }
        double[] swept = org.basaltmc.bukkit.MechanicsNatives.collide(flatten(shapes), corners(box), movement.x, movement.y, movement.z);
        return new Vec3(swept[0], swept[1], swept[2]);
    }
    private static double[] corners(AABB box) {
        return new double[] {box.minX, box.minY, box.minZ, box.maxX, box.maxY, box.maxZ};
    }
    private static double[] flatten(List<VoxelShape> shapes) {
        java.util.ArrayList<AABB> parts = new java.util.ArrayList<>();
        java.util.ArrayList<Integer> owners = new java.util.ArrayList<>();
        for (int shape = 0; shape < shapes.size(); shape++) {
            for (AABB part : shapes.get(shape).toAabbs()) {
                parts.add(part);
                owners.add(shape);
            }
        }
        double[] out = new double[parts.size() * 7];
        for (int at = 0; at < parts.size(); at++) {
            AABB part = parts.get(at);
            System.arraycopy(corners(part), 0, out, at * 7, 6);
            out[at * 7 + 6] = owners.get(at);
        }
        return out;
    }
    public void recordMovement(MoverType moverType, Vec3 movement) {
    }
    protected double getEntityBounciness() {
        return 0.0;
    }
    protected double getEffectiveGravity() {
        return getGravity();
    }
    protected boolean omnidirectionalAirMover() {
        return false;
    }
    protected void applyEffectsFromBlocks() {
        return;
    }
    protected void applyEffectsFromBlocksForLastMovements() {
    }
    public void removeLatestMovementRecording() {
    }
    protected void clearMovementThisTick() {
    }
    public boolean hasMovedHorizontallyRecently() {
        return false;
    }
    public void applyEffectsFromBlocks(Vec3 from, Vec3 to) {
        return;
    }
    protected boolean isAffectedByBlocks() {
        return !isRemoved();
    }
    protected boolean isHorizontalCollisionMinor(Vec3 movement) {
        float radians = getYRot() * ((float) Math.PI / 180.0F);
        double sin = net.minecraft.util.Mth.sin(radians);
        double cos = net.minecraft.util.Mth.cos(radians);
        double along = movement.x * cos - movement.z * sin;
        double across = movement.z * cos + movement.x * sin;
        return Math.abs(along) < 1.0E-5 || Math.abs(across) < 1.0E-5;
    }
    protected void playEntityOnFireExtinguishedSound() {
    }
    public void extinguishFire() {
        setRemainingFireTicks(0);
    }
    protected void processFlappingMovement() {
    }
    public BlockPos getOnPosLegacy() {
        return getOnPos(0.2F);
    }
    public BlockPos getBlockPosBelowThatAffectsMyMovement() {
        return getOnPos(0.500001F);
    }
    public BlockPos getOnPos() {
        return getOnPos(1.0E-5F);
    }
    protected BlockPos getOnPos(float offset) {
        return new BlockPos(net.minecraft.util.Mth.floor(getX()), net.minecraft.util.Mth.floor(getY() - offset), net.minecraft.util.Mth.floor(getZ()));
    }
    protected float getBlockJumpFactor() {
        float here = level().getBlockState(blockPosition()).getBlock().getJumpFactor();
        float below = level().getBlockState(getBlockPosBelowThatAffectsMyMovement()).getBlock().getJumpFactor();
        return here == 1.0F ? below : here;
    }
    protected float getBlockSpeedFactor() {
        net.minecraft.world.level.block.Block block = level().getBlockState(blockPosition()).getBlock();
        float factor = block.getSpeedFactor();
        if (!isInWater() && block != net.minecraft.world.level.block.Blocks.BUBBLE_COLUMN) {
            return factor == 1.0F ? level().getBlockState(getBlockPosBelowThatAffectsMyMovement()).getBlock().getSpeedFactor() : factor;
        }
        return factor;
    }
    protected Vec3 maybeBackOffFromEdge(Vec3 delta, MoverType moverType) {
        return delta;
    }
    protected Vec3 limitPistonMovement(Vec3 vec) {
        return vec;
    }
    public double getAvailableSpaceBelow(double maxDistance) {
        return maxDistance;
    }
    public static Vec3 collideBoundingBox(Entity source, Vec3 movement, AABB boundingBox, Level level, List<VoxelShape> entityColliders) {
        List<VoxelShape> colliders = new java.util.ArrayList<>(entityColliders);
        for (VoxelShape shape : level.getBlockCollisions(source, boundingBox.expandTowards(movement))) {
            colliders.add(shape);
        }
        return collideWithShapes(movement, boundingBox, colliders);
    }
    public static Vec3 collideBoundingBox(CollisionContext source, Vec3 movement, AABB boundingBox, Level level, List<VoxelShape> entityColliders) {
        List<VoxelShape> colliders = new java.util.ArrayList<>(entityColliders);
        for (VoxelShape shape : level.getBlockCollisions(null, boundingBox.expandTowards(movement))) {
            colliders.add(shape);
        }
        return collideWithShapes(movement, boundingBox, colliders);
    }
    public static List<VoxelShape> collectAllColliders(Entity source, Level level, AABB boundingBox) {
        List<VoxelShape> colliders = new java.util.ArrayList<>(level.getEntityCollisions(source, boundingBox));
        for (VoxelShape shape : level.getBlockCollisions(source, boundingBox)) {
            colliders.add(shape);
        }
        return colliders;
    }
    protected float nextStep() {
        return (int) moveDist + 1;
    }
    protected SoundEvent getSwimSound() {
        return net.minecraft.sounds.SoundEvents.byId("entity.generic.swim");
    }
    protected SoundEvent getSwimSplashSound() {
        return net.minecraft.sounds.SoundEvents.byId("entity.generic.splash");
    }
    protected SoundEvent getSwimHighSpeedSplashSound() {
        return net.minecraft.sounds.SoundEvents.byId("entity.generic.splash");
    }
    public boolean collidedWithFluid(FluidState fluidState, BlockPos blockPos, Vec3 from, Vec3 to) {
        return false;
    }
    public boolean collidedWithShapeMovingFrom(Vec3 from, Vec3 to, List<AABB> aabbs) {
        return false;
    }
    protected void onInsideBlock(BlockState state) {
    }
    public BlockPos adjustSpawnLocation(ServerLevel level, BlockPos spawnSuggestion) {
        return spawnSuggestion;
    }
    public void gameEvent(Holder<GameEvent> event, Entity sourceEntity) {

    }
    public void gameEvent(Holder<GameEvent> event) {
        gameEvent(event, this);
    }
    protected void waterSwimSound() {
    }
    protected BlockPos getPrimaryStepSoundBlockPos(BlockPos affectingPos) {
        return affectingPos;
    }
    protected void playCombinationStepSounds(BlockState primaryStepSound, BlockState secondaryStepSound) {
    }
    protected void playMuffledStepSound(BlockState blockState) {
    }
    protected void playStepSound(BlockPos pos, BlockState blockState) {
    }
    protected void playSwimSound(float volume) {
    }
    protected void onFlap() {
    }
    protected boolean isFlapping() {
        return false;
    }
    public void playSound(SoundEvent sound, float volume, float pitch) {
        if (sound != null && !isSilent() && live() != null) {
            live().getWorld().playSound(live().getLocation(), sound.location().toString(), org.bukkit.SoundCategory.NEUTRAL, volume, pitch);
        }
    }
    public void playSound(SoundEvent sound) {
        playSound(sound, 1.0F, 1.0F);
    }
    public boolean isSilent() {
        org.bukkit.entity.Entity live = live();
        return live != null ? live.isSilent() : pendingByte("Silent") != 0;
    }
    public void setSilent(boolean silent) {
        org.bukkit.entity.Entity live = live();
        if (live != null) {
            live.setSilent(silent);
        } else if (pending != null) {
            pending.putBoolean("Silent", silent);
        }
    }
    public boolean isNoGravity() {
        org.bukkit.entity.Entity live = live();
        return live != null ? !live.hasGravity() : pendingByte("NoGravity") != 0;
    }
    public void setNoGravity(boolean noGravity) {
        org.bukkit.entity.Entity live = live();
        if (live != null) {
            live.setGravity(!noGravity);
        } else if (pending != null) {
            pending.putBoolean("NoGravity", noGravity);
        }
    }
    protected double getDefaultGravity() {
        return 0.0;
    }
    public double getGravity() {
        return isNoGravity() ? 0.0 : getDefaultGravity();
    }
    protected void applyGravity() {
        double gravity = getGravity();
        if (gravity != 0.0) {
            setDeltaMovement(getDeltaMovement().add(0.0, -gravity, 0.0));
        }
    }
    protected float getAirDrag() {
        return 0.98F;
    }
    protected MovementEmission getMovementEmission() {
        return MovementEmission.ALL;
    }
    public boolean dampensVibrations() {
        return false;
    }
    public void doCheckFallDamage(double xa, double ya, double za, boolean onGround) {
    }
    protected void checkFallDamage(double ya, boolean onGround, BlockState onState, BlockPos pos) {
    }
    public boolean fireImmune() {
        return false;
    }
    public boolean causeFallDamage(double fallDistance, float damageModifier, DamageSource damageSource) {
        propagateFallToPassengers(fallDistance, damageModifier, damageSource);
        return false;
    }
    protected void propagateFallToPassengers(double fallDistance, float damageModifier, DamageSource damageSource) {
    }
    public boolean isInWater() {
        org.bukkit.entity.Entity live = live();
        return live != null && live.isInWater();
    }
    public boolean isInWaterOrRain() {
        return isInWater() || (live() != null && live().isInRain());
    }
    public boolean isInLiquid() {
        return isInWater() || isInLava();
    }
    public boolean isInFloatableFluid() {
        return isInWater();
    }
    public boolean isUnderWater() {
        return isInWater() && isEyeInFluid(net.minecraft.tags.FluidTags.WATER);
    }
    public boolean isInShallowWater() {
        return isInWater() && !isUnderWater();
    }
    public boolean isInClouds() {
        return false;
    }
    public void updateSwimming() {
    }
    protected boolean updateFluidInteraction() {
        return false;
    }
    protected void doWaterSplashEffect() {
    }
    protected BlockState getBlockStateOnLegacy() {
        return level().getBlockState(getOnPosLegacy());
    }
    public BlockState getBlockStateOn() {
        return level().getBlockState(getOnPos());
    }
    public boolean canSpawnSprintParticle() {
        return isSprinting() && !isInWater() && !isSpectator() && !isCrouching() && !isInLava() && isAlive();
    }
    protected void spawnSprintParticle() {
    }
    public boolean isEyeInFluid(TagKey<Fluid> type) {
        BlockPos eye = BlockPos.containing(getX(), getEyeY(), getZ());
        net.minecraft.world.level.material.FluidState fluid = level().getFluidState(eye);
        return fluid.is(type) && getEyeY() < eye.getY() + fluid.getHeight(level(), eye);
    }
    public boolean isInLava() {
        return false;
    }
    public void moveRelative(float speed, Vec3 input) {
        setDeltaMovement(getDeltaMovement().add(getInputVector(input, speed, getYRot())));
    }
    protected static Vec3 getInputVector(Vec3 input, float speed, float yRot) {
        double lengthSqr = input.lengthSqr();
        if (lengthSqr < 1.0E-7) {
            return Vec3.ZERO;
        }
        Vec3 scaled = (lengthSqr > 1.0 ? input.normalize() : input).scale(speed);
        float sin = net.minecraft.util.Mth.sin(yRot * (float) (Math.PI / 180.0));
        float cos = net.minecraft.util.Mth.cos(yRot * (float) (Math.PI / 180.0));
        return new Vec3(scaled.x * cos - scaled.z * sin, scaled.y, scaled.z * cos + scaled.x * sin);
    }
    public float getLightLevelDependentMagicValue() {
        org.bukkit.entity.Entity api = live();
        float light = api == null ? 15.0F : api.getLocation().getBlock().getLightLevel() / 15.0F;
        return light / (4.0F - 3.0F * light);
    }
    public void absSnapRotationTo(float yRot, float xRot) {
        absSnapTo(getX(), getY(), getZ(), yRot, xRot);
    }
    public void absSnapTo(double x, double y, double z) {
        absSnapTo(x, y, z, getYRot(), getXRot());
    }
    public void snapTo(Vec3 pos) {
        absSnapTo(pos.x, pos.y, pos.z, getYRot(), getXRot());
    }
    public void snapTo(double x, double y, double z) {
        absSnapTo(x, y, z, getYRot(), getXRot());
    }
    public void snapTo(BlockPos spawnPos, float yRot, float xRot) {
        absSnapTo(spawnPos.getX() + 0.5, spawnPos.getY(), spawnPos.getZ() + 0.5, yRot, xRot);
    }
    public void snapTo(Vec3 spawnPos, float yRot, float xRot) {
        absSnapTo(spawnPos.x, spawnPos.y, spawnPos.z, yRot, xRot);
    }
    public void snapTo(double x, double y, double z, float yRot, float xRot) {
        absSnapTo(x, y, z, yRot, xRot);
    }
    public void setOldPosAndRot() {
        setOldPosAndRot(position(), getYRot(), getXRot());
    }
    public void setOldPosAndRot(Vec3 position, float yRot, float xRot) {
        xo = position.x;
        yo = position.y;
        zo = position.z;
        yRotO = yRot;
        xRotO = xRot;
    }
    protected void setOldPos() {
    }
    public void setOldRot() {
    }
    public Vec3 oldPosition() {
        return new Vec3(xo, yo, zo);
    }
    public float distanceTo(Entity entity) {
        return (float) Math.sqrt(distanceToSqr(entity.getX(), entity.getY(), entity.getZ()));
    }
    public double distanceToSqr(double x2, double y2, double z2) {
        double dx = getX() - x2;
        double dy = getY() - y2;
        double dz = getZ() - z2;
        return dx * dx + dy * dy + dz * dz;
    }
    public double distanceToSqr(Entity entity) {
        return distanceToSqr(entity.getX(), entity.getY(), entity.getZ());
    }
    public double distanceToSqr(Vec3 pos) {
        return distanceToSqr(pos.x, pos.y, pos.z);
    }
    public void playerTouch(Player player) {
    }
    public void push(Entity entity) {
        if (isPassengerOfSameVehicle(entity) || entity.noPhysics || noPhysics) {
            return;
        }
        double dx = entity.getX() - getX();
        double dz = entity.getZ() - getZ();
        double dist = net.minecraft.util.Mth.absMax(dx, dz);
        if (dist >= 0.01) {
            dist = Math.sqrt(dist);
            dx /= dist;
            dz /= dist;
            double scale = Math.min(1.0 / dist, 1.0);
            dx *= scale * 0.05;
            dz *= scale * 0.05;
            if (!isVehicle() && isPushable()) {
                push(-dx, 0.0, -dz);
            }
            if (!entity.isVehicle() && entity.isPushable()) {
                entity.push(dx, 0.0, dz);
            }
        }
    }
    public void push(Vec3 impulse) {
        addDeltaMovement(impulse);
    }
    public void pushFromExplosion(Vec3 impulse) {
        push(impulse);
    }
    public void push(double xa, double ya, double za) {
        addDeltaMovement(new Vec3(xa, ya, za));
    }
    protected void markHurt() {
        return;
    }
    public void hurt(DamageSource source, float damage) {
        if (level() != null) {
            hurtServer(level(), source, damage);
        }
    }
    public boolean hurtOrSimulate(DamageSource source, float damage) {
        return level() instanceof ServerLevel server && hurtServer(server, source, damage);
    }
    public boolean hurtServer(ServerLevel level, DamageSource source, float damage) {
        return false;
    }
    public boolean hurtClient(DamageSource source) {
        return false;
    }
    public Vec3 getViewVector(float a) {
        return calculateViewVector(getViewXRot(a), getViewYRot(a));
    }
    public Direction getNearestViewDirection() {
        return Direction.getApproximateNearest(getViewVector(1.0F));
    }
    public float getViewXRot(float a) {
        return getXRot();
    }
    public float getViewYRot(float a) {
        return getYRot();
    }
    public float getXRot(float partialTicks) {
        return getXRot();
    }
    public float getYRot(float partialTicks) {
        return getYRot();
    }
    public static Vec3 calculateViewVector(float xRot, float yRot) {
        float pitch = xRot * (float) (Math.PI / 180.0);
        float yaw = -yRot * (float) (Math.PI / 180.0);
        float cy = Mth.cos(yaw);
        float sy = Mth.sin(yaw);
        float cp = Mth.cos(pitch);
        float sp = Mth.sin(pitch);
        return new Vec3(sy * cp, -sp, cy * cp);
    }
    public static Quaternionf calculateViewQuaternion(float xRot, float yRot) {
        return new Quaternionf().rotationYXZ(-yRot * ((float) Math.PI / 180.0F), xRot * ((float) Math.PI / 180.0F), 0.0F);
    }
    public Vec3 getUpVector(float a) {
        return calculateUpVector(getViewXRot(a), getViewYRot(a));
    }
    protected static Vec3 calculateUpVector(float xRot, float yRot) {
        return calculateViewVector(xRot - 90.0F, yRot);
    }
    public Vec3 getEyePosition() {
        return new Vec3(getX(), getEyeY(), getZ());
    }
    public Vec3 getEyePosition(float partialTickTime) {
        return getEyePosition();
    }
    public Vec3 getLightProbePosition(float partialTickTime) {
        return getEyePosition();
    }
    public HitResult pick(double range, float a, boolean withLiquids) {
        Vec3 eye = getEyePosition(a);
        Vec3 end = eye.add(getViewVector(a).scale(range));
        return level().clip(new ClipContext(eye, end, ClipContext.Block.OUTLINE, withLiquids ? ClipContext.Fluid.ANY : ClipContext.Fluid.NONE, this));
    }
    public boolean canBeHitByProjectile() {
        return isAlive() && isPickable();
    }
    public boolean isPickable() {
        return false;
    }
    public boolean canBePickedFromInside() {
        return false;
    }
    public boolean isPushable() {
        return false;
    }
    public void awardKillScore(Entity victim, DamageSource killingBlow) {

    }
    public boolean shouldRender(double camX, double camY, double camZ) {
        return shouldRenderAtSqrDistance(distanceToSqr(camX, camY, camZ));
    }
    public boolean shouldRenderAtSqrDistance(double distance) {
        return true;
    }
    public boolean saveAsPassenger(ValueOutput output) {
        return false;
    }
    public void postDataManipulated() {

    }
    protected boolean repositionEntityAfterLoad() {
        return true;
    }
    protected String getEncodeId() {
        return getType().toShortString();
    }
    protected void readAdditionalSaveData(ValueInput input) {
        return;
    }
    protected void addAdditionalSaveData(ValueOutput output) {
        return;
    }
    public ItemEntity spawnAtLocation(ServerLevel level, ItemLike resource) {
        return spawnAtLocation(level, new ItemStack(resource));
    }
    public ItemEntity spawnAtLocation(ServerLevel level, ItemStack itemStack) {
        return spawnAtLocation(level, itemStack, 0.0F);
    }
    public ItemEntity spawnAtLocation(ServerLevel level, ItemStack itemStack, Vec3 offset) {
        return spawnAtLocation(level, itemStack, (float) offset.y);
    }
    public ItemEntity spawnAtLocation(ServerLevel level, ItemStack itemStack, float offset) {
        if (itemStack.isEmpty() || live() == null) {
            return null;
        }
        org.bukkit.entity.Item dropped = live().getWorld().dropItem(live().getLocation().add(0.0, offset, 0.0),
            org.bukkit.craftbukkit.inventory.CraftItemStack.asCraftMirror(itemStack));
        return (ItemEntity) ((org.bukkit.craftbukkit.entity.CraftEntity) dropped).getHandle();
    }
    public boolean isAlive() {
        return !isRemoved();
    }
    public boolean isInWall() {
        return false;
    }
    public InteractionResult interact(Player player, InteractionHand hand, Vec3 location) {
        return InteractionResult.PASS;
    }
    public boolean shearOffAllLeashConnections(Player player) {
        return false;
    }
    public boolean dropAllLeashConnections(Player player) {
        return false;
    }
    public boolean canCollideWith(Entity entity) {
        return entity.canBeCollidedWith(this) && !isPassengerOfSameVehicle(entity);
    }
    public boolean canBeCollidedWith(Entity other) {
        return false;
    }
    public void rideTick() {
        setDeltaMovement(Vec3.ZERO);
        tick();
        if (isPassenger()) {
            getVehicle().positionRider(this);
        }
    }
    public void positionRider(Entity passenger) {
        positionRider(passenger, Entity::setPos);
    }
    protected void positionRider(Entity passenger, MoveFunction moveFunction) {
        if (hasPassenger(passenger)) {
            Vec3 at = getPassengerRidingPosition(passenger);
            Vec3 hold = passenger.getVehicleAttachmentPoint(this);
            moveFunction.accept(passenger, at.x - hold.x, at.y - hold.y, at.z - hold.z);
        }
    }
    public void onPassengerTurned(Entity passenger) {
    }
    public Vec3 getVehicleAttachmentPoint(Entity vehicle) {
        return Vec3.ZERO;
    }
    public Vec3 getPassengerRidingPosition(Entity passenger) {
        return position().add(getPassengerAttachmentPoint(passenger, getDimensions(getPose()), 1.0F));
    }
    protected Vec3 getPassengerAttachmentPoint(Entity passenger, EntityDimensions dimensions, float scale) {
        return new Vec3(0.0, dimensions.height() * 0.75F * scale, 0.0);
    }
    protected static Vec3 getDefaultPassengerAttachmentPoint(Entity vehicle, Entity passenger, EntityAttachments attachments) {
        return new Vec3(0.0, vehicle.getBbHeight() * 0.75F, 0.0);
    }
    public boolean startRiding(Entity entity) {
        org.bukkit.entity.Entity live = live();
        org.bukkit.entity.Entity target = entity.live();
        return live != null && target != null && target.addPassenger(live);
    }
    public boolean showVehicleHealth() {
        return false;
    }
    public boolean startRiding(Entity entityToRide, boolean force, boolean sendEventAndTriggers) {
        return live() != null && entityToRide.live() != null && entityToRide.live().addPassenger(live());
    }
    protected boolean canRide(Entity vehicle) {
        return true;
    }
    public void ejectPassengers() {
        org.bukkit.entity.Entity live = live();
        if (live != null) {
            live.eject();
        }
    }
    public boolean doTeamsAllowDamage(Entity other) {
        return true;
    }
    public void removeVehicle() {
        stopRiding();
    }
    public void stopRiding() {
        org.bukkit.entity.Entity live = live();
        if (live != null && live.getVehicle() != null) {
            live.getVehicle().removePassenger(live);
        }
    }
    protected void addPassenger(Entity passenger) {
        if (live() != null && passenger.live() != null) {
            live().addPassenger(passenger.live());
        }
    }
    protected void removePassenger(Entity passenger) {
        if (live() != null && passenger.live() != null) {
            live().removePassenger(passenger.live());
        }
    }
    protected boolean canAddPassenger(Entity passenger) {
        return getPassengers().isEmpty();
    }
    protected boolean couldAcceptPassenger() {
        return true;
    }
    public boolean isInterpolating() {
        return false;
    }
    public void moveOrInterpolateTo(PositionPath position, float yRot, float xRot) {
        moveOrInterpolateTo(position, yRot, xRot, true);
    }
    public void moveOrInterpolateTo(Vec3 position, float yRot, float xRot) {
        snapTo(position, yRot, xRot);
    }
    public void moveOrInterpolateTo(float yRot, float xRot) {
        setRot(yRot, xRot);
    }
    public void moveOrInterpolateTo(PositionPath position) {
        moveOrInterpolateTo(position, getYRot(), getXRot(), false);
    }
    public void moveOrInterpolateTo(PositionPath position, float yRot, float xRot, boolean hasRotation) {
        snapTo(position.endPosition(), hasRotation ? yRot : getYRot(), hasRotation ? xRot : getXRot());
    }
    public void onInterpolationStart(InterpolationHandler interpolation) {
    }
    protected InterpolationHandler createInterpolationHandler() {
        return null;
    }
    public InterpolationHandler getInterpolation() {
        return null;
    }
    public void lerpHeadTo(float yRot, int steps) {
        setYHeadRot(yRot);
    }
    public float getPickRadius() {
        return 0.0F;
    }
    public Vec3 getLookAngle() {
        return calculateViewVector(getXRot(), getYRot());
    }
    public Quaternionf getLookQuaternion() {
        return new Quaternionf().rotationYXZ(-getYRot() * ((float) Math.PI / 180.0F), getXRot() * ((float) Math.PI / 180.0F), 0.0F);
    }
    public Vec3 getHeadLookAngle() {
        return getLookAngle();
    }
    public Vec3 getHandHoldingItemAngle(Item item) {
        return Vec3.ZERO;
    }
    public Vec2 getRotationVector() {
        return new Vec2(getXRot(), getYRot());
    }
    public Vec3 getForward() {
        return calculateViewVector(0.0F, getYRot());
    }
    public void setAsInsidePortal(Portal portal, BlockPos pos) {
    }
    protected void handlePortal() {
    }
    public void teleportToPortalDestination(ServerLevel level, TeleportTransition teleportTransition) {
        teleport(teleportTransition);
    }
    public int getDimensionChangingDelay() {
        return 300;
    }
    public void lerpMotion(Vec3 movement) {
        setDeltaMovement(movement);
    }
    public void handleDamageEvent(DamageSource source) {
    }
    public void handleEntityEvent(byte id) {
    }
    public void animateHurt(float direction) {
    }
    public boolean isOnFire() {
        return getRemainingFireTicks() > 0;
    }
    public boolean isPassenger() {
        return getVehicle() != null;
    }
    public boolean isVehicle() {
        return !getPassengers().isEmpty();
    }
    public boolean dismountsUnderwater() {
        return false;
    }
    public boolean canControlVehicle() {
        return true;
    }
    public void setShiftKeyDown(boolean shiftKeyDown) {
        setSharedFlag(1, shiftKeyDown);
    }
    public boolean isShiftKeyDown() {
        return getSharedFlag(1);
    }
    public boolean isSteppingCarefully() {
        return isDescending();
    }
    public boolean isSuppressingBounce() {
        return false;
    }
    public boolean isDiscrete() {
        return isShiftKeyDown();
    }
    public boolean isDescending() {
        return live() instanceof org.bukkit.entity.Player player && player.isSneaking();
    }
    public boolean isCrouching() {
        return hasPose(Pose.CROUCHING);
    }
    public boolean isSprinting() {
        return getSharedFlag(3);
    }
    public void setSprinting(boolean isSprinting) {
        setSharedFlag(3, isSprinting);
    }
    public boolean isSwimming() {
        return getSharedFlag(4);
    }
    public boolean isVisuallySwimming() {
        return getPose() == Pose.SWIMMING;
    }
    public boolean isVisuallyCrawling() {
        return isVisuallySwimming() && !isInWater();
    }
    public void setSwimming(boolean swimming) {
        setSharedFlag(4, swimming);
    }
    public boolean hasGlowingTag() {
        return getSharedFlag(6);
    }
    public void setGlowingTag(boolean value) {
        setSharedFlag(6, value);
    }
    public boolean isCurrentlyGlowing() {
        return getSharedFlag(6);
    }
    public boolean isInvisible() {
        return getSharedFlag(5);
    }
    public boolean isInvisibleTo(Player player) {
        return isInvisible();
    }
    public boolean isOnRails() {
        return false;
    }
    public void updateDynamicGameEventListener(BiConsumer<DynamicGameEventListener<?>, ServerLevel> action) {
    }
    public PlayerTeam getTeam() {
        return level().getScoreboard().getPlayersTeam(getScoreboardName());
    }
    public boolean isAlliedTo(Entity other) {
        return this == other;
    }
    protected boolean considersEntityAsAlly(Entity other) {
        return false;
    }
    public boolean isAlliedTo(Team other) {
        return false;
    }
    public void setInvisible(boolean invisible) {
        setSharedFlag(5, invisible);
    }
    protected boolean getSharedFlag(int flag) {
        return (sharedFlags() >> flag & 1) != 0;
    }
    protected void setSharedFlag(int flag, boolean value) {
        byte flags = sharedFlags();
        sharedFlags((byte) (value ? flags | 1 << flag : flags & ~(1 << flag)));
    }
    public int getMaxAirSupply() {
        return 300;
    }
    public int getAirSupply() {
        org.bukkit.entity.Entity live = live();
        return live instanceof org.bukkit.entity.LivingEntity living ? living.getRemainingAir() : 300;
    }
    public void setAirSupply(int supply) {
        if (live() instanceof org.bukkit.entity.LivingEntity living) {
            living.setRemainingAir(supply);
        }
    }
    public void clearFreeze() {
        setTicksFrozen(0);
    }
    public int getTicksFrozen() {
        org.bukkit.entity.Entity live = live();
        return live != null ? live.getFreezeTicks() : 0;
    }
    public void setTicksFrozen(int ticks) {
        org.bukkit.entity.Entity live = live();
        if (live != null) {
            live.setFreezeTicks(ticks);
        }
    }
    public float getPercentFrozen() {
        org.bukkit.entity.Entity live = live();
        return live == null ? 0.0F : Math.min(live.getFreezeTicks(), live.getMaxFreezeTicks()) / (float) live.getMaxFreezeTicks();
    }
    public boolean isFullyFrozen() {
        return getTicksFrozen() >= getTicksRequiredToFreeze();
    }
    public int getTicksRequiredToFreeze() {
        return 140;
    }
    public void thunderHit(ServerLevel level, LightningBolt lightningBolt) {
        if (live() != null) {
            live().setFireTicks(Math.max(live().getFireTicks(), 160));
            if (live() instanceof org.bukkit.entity.Damageable damageable) {
                damageable.damage(5.0);
            }
        }
    }
    public void onAboveBubbleColumn(boolean dragDown, BlockPos pos) {
    }
    protected static void handleOnAboveBubbleColumn(Entity entity, boolean dragDown, BlockPos pos) {
        Vec3 motion = entity.getDeltaMovement();
        entity.setDeltaMovement(motion.x, dragDown ? Math.max(-0.9, motion.y - 0.03) : Math.min(1.8, motion.y + 0.1), motion.z);
    }
    protected static void sendBubbleColumnParticles(Level level, BlockPos pos) {
        return;
    }
    public void onInsideBubbleColumn(boolean dragDown) {
    }
    protected static void handleOnInsideBubbleColumn(Entity entity, boolean dragDown) {
        Vec3 motion = entity.getDeltaMovement();
        entity.setDeltaMovement(motion.x, dragDown ? Math.max(-0.3, motion.y - 0.03) : Math.min(0.7, motion.y + 0.06), motion.z);
        entity.resetFallDistance();
    }
    public boolean killedEntity(ServerLevel level, LivingEntity entity, DamageSource source) {
        return true;
    }
    public boolean projectileReceivesSideEffectsOnHit(boolean wasHurt) {
        return false;
    }
    public void checkFallDistanceAccumulation() {
    }
    public void resetFallDistance() {
        if (live() != null) {
            live().setFallDistance(0.0F);
        }
    }
    protected void moveTowardsClosestSpace(double x, double y, double z) {
        return;
    }
    public void makeStuckInBlock(BlockState blockState, Vec3 speedMultiplier) {
        resetFallDistance();
        stuckSpeedMultiplier = speedMultiplier;
    }
    public Component getName() {
        Component custom = getCustomName();
        return custom != null ? custom : getTypeName();
    }
    protected Component getTypeName() {
        Identifier key = EntityType.getKey(type);
        return Component.translatable("entity." + key.getNamespace() + "." + key.getPath().replace('/', '.'));
    }
    public boolean is(Entity other) {
        return this == other;
    }
    public float getYHeadRot() {
        return live() instanceof org.bukkit.entity.LivingEntity living ? living.getEyeLocation().getYaw() : getYRot();
    }
    public void setYHeadRot(float yHeadRot) {
        return;
    }
    public void setYBodyRot(float yBodyRot) {
        return;
    }
    public boolean isAttackable() {
        return true;
    }
    public boolean skipAttackInteraction(Entity source) {
        return false;
    }
    public String toString() {
        return String.format(java.util.Locale.ROOT, "%s['%s'/%d, x=%.2f, y=%.2f, z=%.2f]", getClass().getSimpleName(),
            type == null ? "?" : EntityType.getKey(type).getPath(), getId(), getX(), getY(), getZ());
    }
    protected boolean isInvulnerableToBase(DamageSource source) {
        return isInvulnerable();
    }
    public boolean isInvulnerable() {
        org.bukkit.entity.Entity live = live();
        return live != null ? live.isInvulnerable() : pendingByte("Invulnerable") != 0;
    }
    public boolean isPermanentlyInvulnerable() {
        return isInvulnerable();
    }
    public boolean isTemporarilyInvulnerable() {
        return invulnerableTime > 0;
    }
    public boolean isInvulnerableToPiercingWeapon() {
        return false;
    }
    public void setPermanentlyInvulnerable(boolean permanentlyInvulnerable) {
    }
    public void copyPosition(Entity target) {
        absSnapTo(target.getX(), target.getY(), target.getZ(), target.getYRot(), target.getXRot());
    }
    public void restoreFrom(Entity oldEntity) {
        snapTo(oldEntity.position(), oldEntity.getYRot(), oldEntity.getXRot());
        setDeltaMovement(oldEntity.getDeltaMovement());
    }
    public Entity teleport(TeleportTransition transition) {
        if (transition == null || transition.newLevel() == null) {
            return null;
        }
        Vec3 at = transition.position();
        if (!teleportTo(transition.newLevel(), at.x, at.y, at.z, java.util.Set.of(), transition.yRot(), transition.xRot(), false)) {
            return null;
        }
        setDeltaMovement(transition.deltaMovement());
        return this;
    }
    protected void teleportSpectators(TeleportTransition transition, ServerLevel oldLevel) {
    }
    public void teleportSetPosition(PositionMoveRotation destination, Set<Relative> relatives) {
        teleportSetPosition(PositionMoveRotation.of(this), destination, relatives);
    }
    public void teleportSetPosition(PositionMoveRotation currentValues, PositionMoveRotation destination, Set<Relative> relatives) {
        Vec3 at = destination.position();
        Vec3 base = currentValues.position();
        double x = relatives.contains(Relative.X) ? base.x + at.x : at.x;
        double y = relatives.contains(Relative.Y) ? base.y + at.y : at.y;
        double z = relatives.contains(Relative.Z) ? base.z + at.z : at.z;
        float yaw = relatives.contains(Relative.Y_ROT) ? currentValues.yRot() + destination.yRot() : destination.yRot();
        float pitch = relatives.contains(Relative.X_ROT) ? currentValues.xRot() + destination.xRot() : destination.xRot();
        snapTo(x, y, z, yaw, pitch);
    }
    public void forceSetRotation(float yRot, boolean relativeY, float xRot, boolean relativeX) {
        setYRot(relativeY ? getYRot() + yRot : yRot);
        setXRot(relativeX ? getXRot() + xRot : xRot);
    }
    public void placePortalTicket(BlockPos ticketPosition) {
    }
    protected void removeAfterChangingDimensions() {
        return;
    }
    public Vec3 getRelativePortalPosition(Direction.Axis axis, BlockUtil.FoundRectangle portalArea) {
        return Vec3.ZERO;
    }
    public boolean canUsePortal(boolean ignorePassenger) {
        return (ignorePassenger || !isPassenger()) && isAlive();
    }
    public boolean canTeleport(Level from, Level to) {
        return true;
    }
    public float getBlockExplosionResistance(Explosion explosion, BlockGetter level, BlockPos pos, BlockState block, FluidState fluid, float resistance) {
        return resistance;
    }
    public boolean shouldBlockExplode(Explosion explosion, BlockGetter level, BlockPos pos, BlockState state, float power) {
        return true;
    }
    public int getMaxFallDistance() {
        return 3;
    }
    public boolean isIgnoringBlockTriggers() {
        return false;
    }
    public void fillCrashReportCategory(CrashReportCategory category) {
    }
    public boolean displayFireAnimation() {
        return isOnFire() && !isSpectator();
    }
    public String getScoreboardName() {
        return getStringUUID();
    }
    public boolean isPushedByFluid() {
        return true;
    }
    public static double getViewScale() {
        return 1.0;
    }
    public static void setViewScale(double viewScale) {
    }
    public Component getDisplayName() {
        return getName();
    }
    public void setCustomName(Component name) {
        org.bukkit.entity.Entity live = live();
        if (live != null) {
            live.customName(name == null ? null : io.papermc.paper.adventure.PaperAdventure.asAdventure(name));
        }
    }
    public Component getCustomName() {
        org.bukkit.entity.Entity live = live();
        net.kyori.adventure.text.Component name = live == null ? null : live.customName();
        return name == null ? null : io.papermc.paper.adventure.PaperAdventure.asVanilla(name);
    }
    public boolean hasCustomName() {
        org.bukkit.entity.Entity live = live();
        return live != null && live.customName() != null;
    }
    public void setCustomNameVisible(boolean visible) {
        org.bukkit.entity.Entity live = live();
        if (live != null) {
            live.setCustomNameVisible(visible);
        }
    }
    public boolean isCustomNameVisible() {
        org.bukkit.entity.Entity live = live();
        return live != null && live.isCustomNameVisible();
    }
    public Component belowNameDisplay() {
        return null;
    }
    public boolean teleportTo(ServerLevel level, double x, double y, double z, Set<Relative> relatives, float newYRot, float newXRot, boolean resetCamera) {
        if (live() == null || level == null) {
            return false;
        }
        return live().teleport(new org.bukkit.Location(level.getWorld(), x, y, z, newYRot, newXRot));
    }
    public void dismountTo(double x, double y, double z) {
        absSnapTo(x, y, z, getYRot(), getXRot());
    }
    public void teleportTo(double x, double y, double z) {
        absSnapTo(x, y, z, getYRot(), getXRot());
    }
    public void teleportRelative(double dx, double dy, double dz) {
        absSnapTo(getX() + dx, getY() + dy, getZ() + dz, getYRot(), getXRot());
    }
    public boolean shouldShowName() {
        return false;
    }
    public void onSyncedDataUpdated(List<SynchedEntityData.DataValue<?>> updatedItems) {
        return;
    }
    public void onSyncedDataUpdated(EntityDataAccessor<?> accessor) {
        return;
    }
    protected void fixupDimensions() {
        return;
    }
    public void refreshDimensions() {
        return;
    }
    public boolean fudgePositionAfterSizeChange(EntityDimensions previousDimensions) {
        return false;
    }
    public Direction getDirection() {
        return Direction.fromYRot(getYRot());
    }
    public Direction getMotionDirection() {
        return getDirection();
    }
    protected HoverEvent createHoverEvent() {
        return new HoverEvent.ShowEntity(new HoverEvent.EntityTooltipInfo(getType(), getUUID(), getName()));
    }
    public boolean broadcastToPlayer(ServerPlayer player) {
        return true;
    }
    public AABB getBoundingBox() {
        org.bukkit.entity.Entity live = live();
        if (live != null) {
            org.bukkit.util.BoundingBox box = live.getBoundingBox();
            return new AABB(box.getMinX(), box.getMinY(), box.getMinZ(), box.getMaxX(), box.getMaxY(), box.getMaxZ());
        }
        double halfWidth = getBbWidth() / 2.0;
        return new AABB(getX() - halfWidth, getY(), getZ() - halfWidth, getX() + halfWidth, getY() + getBbHeight(), getZ() + halfWidth);
    }
    public AABB getInterpolatedBoundingBox(float partialTicks) {
        return getBoundingBox();
    }
    public void setBoundingBox(AABB bb) {
        return;
    }
    public float getEyeHeight(Pose pose) {
        return getDimensions(pose).eyeHeight();
    }
    public float getEyeHeight() {
        org.bukkit.entity.Entity live = live();
        if (live instanceof org.bukkit.entity.LivingEntity living) {
            return (float) living.getEyeHeight();
        }
        return getBbHeight() * 0.85F;
    }
    public SlotAccess getSlot(int slot) {
        return SlotAccess.of(() -> ItemStack.EMPTY, stack -> {});
    }
    public boolean ignoreExplosion(Explosion explosion) {
        return false;
    }
    public void startSeenByPlayer(ServerPlayer player) {
    }
    public void stopSeenByPlayer(ServerPlayer player) {
    }
    public float rotate(Rotation rotation) {
        float yaw = Mth.wrapDegrees(getYRot());
        return switch (rotation) {
            case CLOCKWISE_180 -> yaw + 180.0F;
            case COUNTERCLOCKWISE_90 -> yaw + 270.0F;
            case CLOCKWISE_90 -> yaw + 90.0F;
            default -> yaw;
        };
    }
    public float mirror(Mirror mirror) {
        float yaw = Mth.wrapDegrees(getYRot());
        return switch (mirror) {
            case FRONT_BACK -> -yaw;
            case LEFT_RIGHT -> 180.0F - yaw;
            default -> yaw;
        };
    }
    public ProjectileDeflection deflection(Projectile projectile) {
        return ProjectileDeflection.NONE;
    }
    public LivingEntity getControllingPassenger() {
        return null;
    }
    public boolean hasControllingPassenger() {
        return getControllingPassenger() != null;
    }
    public List<Entity> getPassengers() {
        org.bukkit.entity.Entity live = live();
        List<Entity> out = new java.util.ArrayList<>();
        if (live != null) {
            for (org.bukkit.entity.Entity passenger : live.getPassengers()) {
                out.add(wrap(passenger));
            }
        }
        return out;
    }
    public Entity getFirstPassenger() {
        List<Entity> all = getPassengers();
        return all.isEmpty() ? null : all.get(0);
    }
    public boolean hasPassenger(Entity entity) {
        return getPassengers().contains(entity);
    }
    public boolean hasPassenger(Predicate<Entity> test) {
        return getPassengers().stream().anyMatch(test);
    }
    public Stream<Entity> getSelfAndPassengers() {
        return Stream.concat(Stream.of(this), java.util.stream.StreamSupport.stream(getIndirectPassengers().spliterator(), false));
    }
    public Stream<Entity> getPassengersAndSelf() {
        return Stream.concat(java.util.stream.StreamSupport.stream(getIndirectPassengers().spliterator(), false), Stream.of(this));
    }
    public Iterable<Entity> getIndirectPassengers() {
        java.util.List<Entity> all = new java.util.ArrayList<>();
        for (Entity passenger : getPassengers()) {
            all.add(passenger);
            passenger.getIndirectPassengers().forEach(all::add);
        }
        return all;
    }
    public int countPlayerPassengers() {
        return (int) getPassengers().stream().filter(passenger -> passenger instanceof net.minecraft.world.entity.player.Player).count();
    }
    public boolean hasExactlyOnePlayerPassenger() {
        return countPlayerPassengers() == 1;
    }
    public Entity getRootVehicle() {
        Entity root = this;
        while (root.getVehicle() != null) {
            root = root.getVehicle();
        }
        return root;
    }
    public boolean isPassengerOfSameVehicle(Entity other) {
        Entity mine = this;
        while (mine.getVehicle() != null) {
            mine = mine.getVehicle();
        }
        Entity theirs = other;
        while (theirs.getVehicle() != null) {
            theirs = theirs.getVehicle();
        }
        return mine == theirs || mine.getId() == theirs.getId();
    }
    public boolean hasIndirectPassenger(Entity entity) {
        for (Entity passenger : getIndirectPassengers()) {
            if (passenger.getId() == entity.getId()) {
                return true;
            }
        }
        return false;
    }
    public boolean isLocalInstanceAuthoritative() {
        return true;
    }
    protected boolean isLocalClientAuthoritative() {
        return false;
    }
    public boolean isClientAuthoritative() {
        return false;
    }
    public MoveSimulationType getMoveSimulationType() {
        return MoveSimulationType.AUTHORITATIVE_SIDE;
    }
    public boolean canSimulateMovement() {
        return true;
    }
    public boolean isEffectiveAi() {
        return true;
    }
    protected static Vec3 getCollisionHorizontalEscapeVector(double colliderWidth, double collidingWidth, float directionDegrees) {
        double reach = (colliderWidth + collidingWidth + 1.0E-5F) / 2.0;
        float x = -net.minecraft.util.Mth.sin(directionDegrees * (float) (Math.PI / 180.0));
        float z = net.minecraft.util.Mth.cos(directionDegrees * (float) (Math.PI / 180.0));
        float scale = Math.max(Math.abs(x), Math.abs(z));
        return new Vec3(x * reach / scale, 0.0, z * reach / scale);
    }
    public Vec3 getDismountLocationForPassenger(LivingEntity passenger) {
        return new Vec3(getX(), getBoundingBox().maxY, getZ());
    }
    public Entity getVehicle() {
        org.bukkit.entity.Entity live = live();
        return live == null || live.getVehicle() == null ? null : wrap(live.getVehicle());
    }
    public Entity getControlledVehicle() {
        return null;
    }
    public PushReaction getPistonPushReaction() {
        return PushReaction.PUSH_PULL;
    }
    public SoundSource getSoundSource() {
        return net.minecraft.sounds.SoundSource.NEUTRAL;
    }
    protected int getFireImmuneTicks() {
        return 0;
    }
    public CommandSourceStack createCommandSourceStackForNameResolution(ServerLevel level) {
        return new CommandSourceStack(net.minecraft.commands.CommandSource.NULL, position(), getRotationVector(), level, net.minecraft.server.permissions.LevelBasedPermissionSet.GAMEMASTER, level.getServer(), this);
    }
    public void lookAt(EntityAnchorArgument.Anchor anchor, Vec3 pos) {
        Vec3 from = anchor.apply(this);
        double dx = pos.x - from.x;
        double dy = pos.y - from.y;
        double dz = pos.z - from.z;
        double horizontal = Math.sqrt(dx * dx + dz * dz);
        setXRot(Mth.wrapDegrees((float) -(Mth.atan2(dy, horizontal) * 180.0F / (float) Math.PI)));
        setYRot(Mth.wrapDegrees((float) (Mth.atan2(dz, dx) * 180.0F / (float) Math.PI) - 90.0F));
    }
    public float getPreciseBodyRotation(float partial) {
        return getYRot();
    }
    public boolean touchingUnloadedChunk() {
        return false;
    }
    public double getFluidHeight(TagKey<Fluid> type) {
        AABB box = getBoundingBox().deflate(0.001);
        double height = 0.0;
        for (int x = net.minecraft.util.Mth.floor(box.minX); x < net.minecraft.util.Mth.ceil(box.maxX); x++) {
            for (int y = net.minecraft.util.Mth.floor(box.minY); y < net.minecraft.util.Mth.ceil(box.maxY); y++) {
                for (int z = net.minecraft.util.Mth.floor(box.minZ); z < net.minecraft.util.Mth.ceil(box.maxZ); z++) {
                    BlockPos pos = new BlockPos(x, y, z);
                    net.minecraft.world.level.material.FluidState fluid = level().getFluidState(pos);
                    if (fluid.is(type)) {
                        double top = y + fluid.getHeight(level(), pos);
                        if (top >= box.minY) {
                            height = Math.max(top - box.minY, height);
                        }
                    }
                }
            }
        }
        return height;
    }
    public double getFluidJumpThreshold() {
        return getEyeHeight() < 0.4 ? 0.0 : 0.4;
    }
    public float getBbWidth() {
        org.bukkit.entity.Entity live = live();
        return live != null ? (float) live.getWidth() : 0.6F;
    }
    public float getBbHeight() {
        org.bukkit.entity.Entity live = live();
        return live != null ? (float) live.getHeight() : 1.8F;
    }
    public Packet<ClientGamePacketListener> getAddEntityPacket(ServerEntity serverEntity) {
        return new net.minecraft.network.protocol.game.ClientboundAddEntityPacket(this, serverEntity);
    }
    public EntityDimensions getDimensions(Pose pose) {
        return EntityDimensions.scalable(getBbWidth(), getBbHeight());
    }
    public EntityAttachments getAttachments() {
        return getDimensions(getPose()).attachments();
    }
    public Vec3 position() {
        return new Vec3(getX(), getY(), getZ());
    }
    public Vec3 trackingPosition() {
        return position();
    }
    public BlockPos blockPosition() {
        return new BlockPos(getBlockX(), getBlockY(), getBlockZ());
    }
    public BlockState getInBlockState() {
        return level().getBlockState(blockPosition());
    }
    public ChunkPos chunkPosition() {
        return new ChunkPos(blockPosition().getX() >> 4, blockPosition().getZ() >> 4);
    }
    public Vec3 getDeltaMovement() {
        return motion();
    }
    public void setDeltaMovement(Vec3 deltaMovement) {
        setMotion(deltaMovement.x, deltaMovement.y, deltaMovement.z);
    }
    public void addDeltaMovement(Vec3 momentum) {
        Vec3 now = motion();
        setMotion(now.x + momentum.x, now.y + momentum.y, now.z + momentum.z);
    }
    public void setDeltaMovement(double xd, double yd, double zd) {
        setMotion(xd, yd, zd);
    }
    public int getBlockX() {
        return Mth.floor(getX());
    }
    public double getX() {
        return posComponent(0);
    }
    public double getX(double progress) {
        return getX() + getBbWidth() * progress;
    }
    public double getRandomX(double spread) {
        return getX() + getBbWidth() * spread * (2.0 * getRandom().nextDouble() - 1.0);
    }
    public int getBlockY() {
        return Mth.floor(getY());
    }
    public double getY() {
        return posComponent(1);
    }
    public double getY(double progress) {
        return getY() + getBbHeight() * progress;
    }
    public double getRandomY(double spread) {
        return getY((2.0 * getRandom().nextDouble() - 1.0) * spread);
    }
    public double getRandomY() {
        return getY(getRandom().nextDouble());
    }
    public double getEyeY() {
        return getY() + getEyeHeight();
    }
    public int getBlockZ() {
        return Mth.floor(getZ());
    }
    public double getZ() {
        return posComponent(2);
    }
    public double getZ(double progress) {
        return getZ() + getBbWidth() * progress;
    }
    public double getRandomZ(double spread) {
        return getZ() + getBbWidth() * spread * (2.0 * getRandom().nextDouble() - 1.0);
    }
    public void setPosRaw(double x, double y, double z) {
        absSnapTo(x, y, z, getYRot(), getXRot());
    }
    public void checkDespawn() {
        return;
    }
    public Vec3[] getQuadLeashHolderOffsets() {
        return new Vec3[] {Vec3.ZERO, Vec3.ZERO, Vec3.ZERO, Vec3.ZERO};
    }
    public boolean supportQuadLeashAsHolder() {
        return false;
    }
    public void notifyLeashHolder(Leashable entity) {
        return;
    }
    public void notifyLeasheeRemoved(Leashable entity) {
        return;
    }
    public Vec3 getRopeHoldPosition(float partialTickTime) {
        return position().add(0.0, getBbHeight() * 0.5, 0.0);
    }
    public void recreateFromPacket(ClientboundAddEntityPacket packet) {
        return;
    }
    public ItemStack getPickResult() {
        return null;
    }
    public void setIsInPowderSnow(boolean isInPowderSnow) {
        this.isInPowderSnow = isInPowderSnow;
    }
    public boolean canFreeze() {
        return live() != null && !live().isInvulnerable();
    }
    public boolean isFreezing() {
        return live() != null && live().getFreezeTicks() > 0;
    }
    public float getYRot() {
        return rotComponent(0);
    }
    public float getVisualRotationYInDegrees() {
        return getYRot();
    }
    public void setYRot(float yRot) {
        setRot(yRot, getXRot());
    }
    public float getXRot() {
        return rotComponent(1);
    }
    public void setXRot(float xRot) {
        setRot(getYRot(), xRot);
    }
    public PositionAndRotation storePositionAndRotation() {
        return PositionAndRotation.of(position(), getYRot(), getXRot());
    }
    public PositionAndRotation getClientPositionAndRotation() {
        return PositionAndRotation.of(getClientPosition(), getYRot(), getXRot());
    }
    public Vec3 getClientPosition() {
        return position();
    }
    public boolean canSprint() {
        return false;
    }
    public float maxUpStep() {
        return 0.0F;
    }
    public void onExplosionHit(Entity explosionCausedBy) {
    }
    public RemovalReason getRemovalReason() {
        return null;
    }
    public void setRemoved(RemovalReason reason) {
        return;
    }
    protected void unsetRemoved() {
        return;
    }
    public void setLevelCallback(EntityInLevelCallback levelCallback) {
    }
    public boolean shouldBeSaved() {
        return true;
    }
    public boolean isAlwaysTicking() {
        return false;
    }
    public boolean mayInteract(ServerLevel level, BlockPos pos) {
        return true;
    }
    public boolean isFlyingVehicle() {
        return false;
    }
    protected void setLevel(Level level) {
        return;
    }
    public DamageSources damageSources() {
        if (damageSourcesCache == null) {
            damageSourcesCache = new DamageSources(null);
        }
        return damageSourcesCache;
    }
    protected void lerpPositionAndRotationStep(int stepsToTarget, double targetX, double targetY, double targetZ, double targetYRot, double targetXRot) {
    }
    public RandomSource getRandom() {
        return random;
    }
    public Vec3 getKnownMovement() {
        return getDeltaMovement();
    }
    public Vec3 getKnownSpeed() {
        return getDeltaMovement();
    }
    public ItemStack getWeaponItem() {
        return null;
    }
    public Optional<ResourceKey<LootTable>> getLootTable() {
        return Optional.empty();
    }
    protected void applyImplicitComponents(DataComponentGetter components) {
        return;
    }
    public void applyComponentsFromItemStack(ItemStack stack) {
        return;
    }
    public <T> T get(DataComponentType<? extends T> type) {
        return null;
    }
    protected static <T> T castComponentValue(DataComponentType<T> type, Object value) {
        return (T) value;
    }
    public <T> void setComponent(DataComponentType<T> type, T value) {
        return;
    }
    protected <T> boolean applyImplicitComponent(DataComponentType<T> type, T value) {
        return false;
    }
    protected <T> boolean applyImplicitComponentIfPresent(DataComponentGetter components, DataComponentType<T> type) {
        T value = components.get(type);
        return value != null && applyImplicitComponent(type, value);
    }
    public ProblemReporter.PathElement problemPath() {
        return new EntityPathElement(this);
    }
    public void registerDebugValues(ServerLevel level, DebugValueSource.Registration registration) {
    }
    public AABB getFluidInteractionBox() {
        return getBoundingBox();
    }
    public void setInvulnerableTime(int invulnerableTime) {
        this.invulnerableTime = invulnerableTime;
    }
    public int getInvulnerableTime() {
        return invulnerableTime;
    }
    protected AABB modifyPassengerFluidInteractionBox(AABB passengerBox) {
        return passengerBox;
    }
    static record Movement(Vec3 from, Vec3 to, Optional<Vec3> axisDependentOriginalMovement) {
        public Movement(Vec3 from, Vec3 to, Vec3 axisDependentOriginalMovement) {
            this((Vec3) null, (Vec3) null, (Optional) null);
        }
        public Movement(Vec3 from, Vec3 to) {
            this((Vec3) null, (Vec3) null, (Optional) null);
        }
    }
    public static enum MovementEmission {
        NONE, SOUNDS, EVENTS, ALL;
        public boolean emitsAnything() {
            return this != NONE;
        }
        public boolean emitsEvents() {
            return this == EVENTS || this == ALL;
        }
        public boolean emitsSounds() {
            return this == SOUNDS || this == ALL;
        }
    }
    public static enum RemovalReason {
        KILLED, DISCARDED, UNLOADED_TO_CHUNK, UNLOADED_WITH_PLAYER, CHANGED_DIMENSION;
        public boolean shouldDestroy() {
        return this == KILLED || this == DISCARDED;
        }
        public boolean shouldSave() {
        return this == UNLOADED_TO_CHUNK;
        }
    }
    static record EntityPathElement(Entity entity) implements ProblemReporter.PathElement {
        public String get() {
            return "entity " + entity.getId();
        }
    }
    public @interface Flags {
    }
    public interface MoveFunction {
        void accept(Entity target, double x, double y, double z);
    }

    private final VecDeltaCodec positionCodec = new VecDeltaCodec();
    private DamageSources damageSourcesCache;

}
