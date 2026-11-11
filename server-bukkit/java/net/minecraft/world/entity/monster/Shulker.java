// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.monster;
import java.util.Optional;
import net.minecraft.core.BlockPos;
import net.minecraft.core.Direction;
import net.minecraft.core.component.DataComponentGetter;
import net.minecraft.core.component.DataComponentType;
import net.minecraft.network.protocol.game.ClientboundAddEntityPacket;
import net.minecraft.network.syncher.EntityDataAccessor;
import net.minecraft.network.syncher.SynchedEntityData;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.sounds.SoundEvent;
import net.minecraft.sounds.SoundSource;
import net.minecraft.world.DifficultyInstance;
import net.minecraft.world.damagesource.DamageSource;
import net.minecraft.world.entity.Entity;
import net.minecraft.world.entity.EntityDimensions;
import net.minecraft.world.entity.EntitySpawnReason;
import net.minecraft.world.entity.EntityType;
import net.minecraft.world.entity.InterpolationHandler;
import net.minecraft.world.entity.LivingEntity;
import net.minecraft.world.entity.Mob;
import net.minecraft.world.entity.MoverType;
import net.minecraft.world.entity.Pose;
import net.minecraft.world.entity.SpawnGroupData;
import net.minecraft.world.entity.ai.attributes.AttributeSupplier;
import net.minecraft.world.entity.ai.control.BodyRotationControl;
import net.minecraft.world.entity.ai.control.LookControl;
import net.minecraft.world.entity.ai.goal.Goal;
import net.minecraft.world.entity.ai.goal.target.NearestAttackableTargetGoal;
import net.minecraft.world.entity.animal.golem.AbstractGolem;
import net.minecraft.world.entity.player.Player;
import net.minecraft.world.item.DyeColor;
import net.minecraft.world.level.Level;
import net.minecraft.world.level.ServerLevelAccessor;
import net.minecraft.world.level.storage.ValueInput;
import net.minecraft.world.level.storage.ValueOutput;
import net.minecraft.world.phys.AABB;
import net.minecraft.world.phys.Vec3;
public class Shulker extends AbstractGolem implements Enemy {
    protected static final EntityDataAccessor<Direction> DATA_ATTACH_FACE_ID = net.minecraft.network.syncher.EntityDataSerializers.DIRECTION.createAccessor(16);
    protected static final EntityDataAccessor<Byte> DATA_PEEK_ID = net.minecraft.network.syncher.EntityDataSerializers.BYTE.createAccessor(17);
    protected static final EntityDataAccessor<Byte> DATA_COLOR_ID = net.minecraft.network.syncher.EntityDataSerializers.BYTE.createAccessor(18);
    public static final int MAX_TELEPORT_DISTANCE = 8;
    public Shulker(EntityType<? extends Shulker> type, Level level) {
        super(type, level);
        this.lookControl = new ShulkerLookControl(this);
    }
    int getRawPeekAmount() {
        return live() instanceof org.bukkit.entity.Shulker api ? Math.round(api.getPeek() * 100.0F) : 0;
    }
    void setRawPeekAmount(int amount) {
        if (live() instanceof org.bukkit.entity.Shulker api) { api.setPeek(amount * 0.01F); }
    }
    boolean canStayAt(BlockPos pos, Direction face) {
        return level().getBlockState(pos.relative(face)).isSolid();
    }
    private boolean peaceful() {
        return live().getWorld().getDifficulty() == org.bukkit.Difficulty.PEACEFUL;
    }
    protected void registerGoals() {
        goalSelector.addGoal(1, new net.minecraft.world.entity.ai.goal.LookAtPlayerGoal(this, net.minecraft.world.entity.player.Player.class, 8.0F, 0.02F, true));
        goalSelector.addGoal(8, new net.minecraft.world.entity.ai.goal.RandomLookAroundGoal(this));
    }
    protected Entity.MovementEmission getMovementEmission() {
        return super.getMovementEmission();
    }
    public SoundSource getSoundSource() {
        return super.getSoundSource();
    }
    protected SoundEvent getAmbientSound() {
        return net.minecraft.sounds.SoundEvents.byId("entity.shulker.ambient");
    }
    public void playAmbientSound() {
    }
    protected SoundEvent getDeathSound() {
        return net.minecraft.sounds.SoundEvents.byId("entity.shulker.death");
    }
    protected SoundEvent getHurtSound(DamageSource source) {
        return net.minecraft.sounds.SoundEvents.byId("entity.shulker.hurt");
    }
    protected void defineSynchedData(SynchedEntityData.Builder entityData) {
    }
    public static AttributeSupplier.Builder createAttributes() {
        return net.minecraft.world.entity.Mob.createMobAttributes()
            .add(net.minecraft.world.entity.ai.attributes.Attributes.MAX_HEALTH, 30.0);
    }
    protected BodyRotationControl createBodyControl() {
        return new ShulkerBodyRotationControl(this);
    }
    protected void readAdditionalSaveData(ValueInput input) {
    }
    protected void addAdditionalSaveData(ValueOutput output) {
    }
    public void tick() {
    }
    public EntityDimensions getDefaultDimensions(Pose pose) {
        return net.minecraft.world.entity.EntityDimensions.scalable(getBbWidth(), getBbHeight());
    }
    public static AABB getProgressAabb(float size, Direction direction, float progressTo, Vec3 position) {
        return getProgressDeltaAabb(size, direction, -1.0F, progressTo, position);
    }
    public static AABB getProgressDeltaAabb(float size, Direction direction, float progressFrom, float progressTo, Vec3 position) {
        double[] box = org.basaltmc.bukkit.MechanicsNatives.shulkerBox(size, direction.ordinal(), progressFrom, progressTo,
            position.x, position.y, position.z);
        return new AABB(box[0], box[1], box[2], box[3], box[4], box[5]);
    }
    public SpawnGroupData finalizeSpawn(ServerLevelAccessor level, DifficultyInstance difficulty, EntitySpawnReason spawnReason, SpawnGroupData groupData) {
        return groupData;
    }
    protected Direction findAttachableSurface(BlockPos target) {
        for (Direction direction : Direction.values()) {
            if (canStayAt(target, direction)) {
                return direction;
            }
        }
        return null;
    }
    protected boolean teleportSomewhere() {
        return false;
    }
    public boolean hurtServer(ServerLevel level, DamageSource source, float damage) {
        return super.hurtServer(level, source, damage);
    }
    public boolean canBeCollidedWith(Entity other) {
        return false;
    }
    public Direction getAttachFace() {
        return live() instanceof org.bukkit.entity.Shulker api ? Direction.valueOf(api.getAttachedFace().name()) : Direction.DOWN;
    }
    public void onSyncedDataUpdated(EntityDataAccessor<?> accessor) {
    }
    public float getClientPeekAmount(float a) {
        return getRawPeekAmount() * 0.01F;
    }
    public void recreateFromPacket(ClientboundAddEntityPacket packet) {
        super.recreateFromPacket(packet);
    }
    public int getMaxHeadXRot() {
        return super.getMaxHeadXRot();
    }
    public int getMaxHeadYRot() {
        return super.getMaxHeadYRot();
    }
    public void push(Entity entity) {
        
    }
    public Vec3 getRenderPosition(float a) {
        return position();
    }
    protected float sanitizeScale(float scale) {
        return 0;
    }
    public Optional<DyeColor> getVariant() {
        return java.util.Optional.ofNullable(live() instanceof org.bukkit.entity.Shulker api && api.getColor() != null ? DyeColor.byId(api.getColor().ordinal()) : null);
    }
    public DyeColor getColor() {
        return getVariant().orElse(null);
    }
    public <T> T get(DataComponentType<? extends T> type) {
        return super.get(type);
    }
    protected void applyImplicitComponents(DataComponentGetter components) {
        }
    protected <T> boolean applyImplicitComponent(DataComponentType<T> type, T value) {
        return false;
    }
    class ShulkerLookControl extends LookControl {
        public ShulkerLookControl(Mob mob) {
            super(mob);
        }
        protected void clampHeadRotationToBody() {
        }
        protected Optional<Float> getYRotD() {
            Direction face = getAttachFace().getOpposite();
            Vec3 view = new Vec3(wantedX - getX(), wantedY - getY(), wantedZ - getZ());
            if (face.getAxis() == Direction.Axis.Y) {
                return Optional.empty();
            }
            return Optional.of((float) (Math.atan2(view.z, view.x) * (180.0F / Math.PI)) - 90.0F);
        }
        protected Optional<Float> getXRotD() {
            return Optional.of(0.0F);
        }
    }
    static class ShulkerBodyRotationControl extends BodyRotationControl {
        public ShulkerBodyRotationControl(Mob mob) {
            super(mob);
        }
        public void clientTick() {
        }
    }
    class ShulkerPeekGoal extends Goal {
        private int peekTime;
        private ShulkerPeekGoal() {
        }
        public boolean canUse() {
            return getTarget() == null && random.nextInt(reducedTickDelay(40)) == 0 && canStayAt(blockPosition(), getAttachFace());
        }
        public boolean canContinueToUse() {
            return getTarget() == null && peekTime > 0;
        }
        public void start() {
            peekTime = adjustedTickDelay(20 * (1 + random.nextInt(3)));
            setRawPeekAmount(30);
        }
        public void stop() {
            if (getTarget() == null) {
                setRawPeekAmount(0);
            }
        }
        public void tick() {
            peekTime--;
        }
    }
    class ShulkerAttackGoal extends Goal {
        private int attackTime;
        public ShulkerAttackGoal() {
            setFlags(java.util.EnumSet.of(Goal.Flag.MOVE, Goal.Flag.LOOK));
        }
        public boolean canUse() {
            LivingEntity target = getTarget();
            return target != null && target.isAlive() && !peaceful();
        }
        public void start() {
            attackTime = 20;
            setRawPeekAmount(100);
        }
        public void stop() {
            setRawPeekAmount(0);
        }
        public boolean requiresUpdateEveryTick() {
            return true;
        }
        public void tick() {
            LivingEntity target = getTarget();
            if (peaceful() || target == null) {
                return;
            }
            attackTime--;
            getLookControl().setLookAt(target, 180.0F, 180.0F);
            if (distanceToSqr(target) < 400.0) {
                if (attackTime <= 0) {
                    attackTime = 20 + random.nextInt(10) * 20 / 2;
                    org.bukkit.entity.ShulkerBullet bullet = live().getWorld().spawn(live().getLocation().add(0, 0.5, 0),
                        org.bukkit.entity.ShulkerBullet.class);
                    bullet.setShooter((org.bukkit.entity.Shulker) live());
                    bullet.setTarget((org.bukkit.entity.Entity) target.getBukkitEntity());
                }
            } else {
                setTarget(null);
            }
        }
    }
    class ShulkerNearestAttackGoal extends NearestAttackableTargetGoal<Player> {
        public ShulkerNearestAttackGoal(Shulker mob) {
            super(mob, Player.class, true);
        }
        public boolean canUse() {
            return !peaceful() && super.canUse();
        }
        protected AABB getTargetSearchArea(double followDistance) {
            Direction.Axis axis = getAttachFace().getAxis();
            if (axis == Direction.Axis.X) {
                return getBoundingBox().inflate(4.0, followDistance, followDistance);
            }
            return axis == Direction.Axis.Z ? getBoundingBox().inflate(followDistance, followDistance, 4.0)
                : getBoundingBox().inflate(followDistance, 4.0, followDistance);
        }
    }
    class ShulkerDefenseAttackGoal extends NearestAttackableTargetGoal<LivingEntity> {
        public ShulkerDefenseAttackGoal(Shulker mob) {
            super(mob, LivingEntity.class, 10, true, false, (target, level) -> target instanceof net.minecraft.world.entity.monster.Enemy);
        }
        public boolean canUse() {
            return !peaceful() && super.canUse();
        }
        protected AABB getTargetSearchArea(double followDistance) {
            Direction.Axis axis = getAttachFace().getAxis();
            if (axis == Direction.Axis.X) {
                return getBoundingBox().inflate(4.0, followDistance, followDistance);
            }
            return axis == Direction.Axis.Z ? getBoundingBox().inflate(followDistance, followDistance, 4.0)
                : getBoundingBox().inflate(followDistance, 4.0, followDistance);
        }
    }
}
