// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.animal.rabbit;
import net.minecraft.world.level.block.state.BlockState;
import net.minecraft.world.level.block.Blocks;
import com.mojang.serialization.Codec;
import io.netty.buffer.ByteBuf;
import net.minecraft.core.BlockPos;
import net.minecraft.core.component.DataComponentGetter;
import net.minecraft.core.component.DataComponentType;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.network.syncher.SynchedEntityData;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.sounds.SoundEvent;
import net.minecraft.sounds.SoundSource;
import net.minecraft.util.RandomSource;
import net.minecraft.util.StringRepresentable;
import net.minecraft.world.DifficultyInstance;
import net.minecraft.world.damagesource.DamageSource;
import net.minecraft.world.entity.AgeableMob;
import net.minecraft.world.entity.AnimationState;
import net.minecraft.world.entity.EntityDimensions;
import net.minecraft.world.entity.EntitySpawnReason;
import net.minecraft.world.entity.EntityType;
import net.minecraft.world.entity.Leashable;
import net.minecraft.world.entity.LivingEntity;
import net.minecraft.world.entity.Pose;
import net.minecraft.world.entity.SpawnGroupData;
import net.minecraft.world.entity.ai.attributes.AttributeSupplier;
import net.minecraft.world.entity.ai.control.JumpControl;
import net.minecraft.world.entity.ai.control.MoveControl;
import net.minecraft.world.entity.ai.goal.AvoidEntityGoal;
import net.minecraft.world.entity.ai.goal.MoveToBlockGoal;
import net.minecraft.world.entity.ai.goal.PanicGoal;
import net.minecraft.world.entity.animal.Animal;
import net.minecraft.world.item.ItemStack;
import net.minecraft.world.level.Level;
import net.minecraft.world.level.LevelAccessor;
import net.minecraft.world.level.LevelReader;
import net.minecraft.world.level.ServerLevelAccessor;
import net.minecraft.world.level.storage.ValueInput;
import net.minecraft.world.level.storage.ValueOutput;
import net.minecraft.world.phys.Vec3;
public class Rabbit extends Animal {
    public static final double STROLL_SPEED_MOD = 0.6;
    public static final double BREED_SPEED_MOD = 0.8;
    public static final double FOLLOW_SPEED_MOD = 1.0;
    public static final double FLEE_SPEED_MOD = 2.2;
    public static final double ATTACK_SPEED_MOD = 1.4;
    public final AnimationState hopAnimationState = null;
    public final AnimationState idleHeadTiltAnimationState = null;
    public Rabbit(EntityType<? extends Rabbit> type, Level level) {
        super(type, level);
        this.moveControl = new RabbitMoveControl<>(this);
    }
    int jumpDuration;
    int jumpTicks;
    int moreCarrotTicks;
    boolean wantsMoreFood() {
        return moreCarrotTicks <= 0;
    }
    void setMoreCarrotTicks(int ticks) {
        moreCarrotTicks = ticks;
    }
    protected void registerGoals() {
        goalSelector.addGoal(1, new net.minecraft.world.entity.ai.goal.FloatGoal(this));
        goalSelector.addGoal(2, new net.minecraft.world.entity.ai.goal.BreedGoal(this, 0.8));
        goalSelector.addGoal(3, new net.minecraft.world.entity.ai.goal.TemptGoal(this, 1.0, stack -> isFood(stack), false));
        goalSelector.addGoal(6, new net.minecraft.world.entity.ai.goal.WaterAvoidingRandomStrollGoal(this, 0.6));
        goalSelector.addGoal(11, new net.minecraft.world.entity.ai.goal.LookAtPlayerGoal(this, net.minecraft.world.entity.player.Player.class, 10.0F));
    }
    protected EntityDimensions getDefaultDimensions(Pose pose) {
        return net.minecraft.world.entity.EntityDimensions.scalable(getBbWidth(), getBbHeight());
    }
    protected float getJumpPower() {
        return 0;
    }
    public float getJumpCompletion(float a) {
        return jumpDuration == 0 ? 0.0F : (jumpTicks + a) / jumpDuration;
    }
    public void setSpeedModifier(double speed) {
        getNavigation().setSpeedModifier(speed);
        getMoveControl().setWantedPosition(getMoveControl().getWantedX(), getMoveControl().getWantedY(), getMoveControl().getWantedZ(), speed);
    }
    public void setJumping(boolean jump) {
        if (live() instanceof org.bukkit.entity.Rabbit api) { api.setJumping(jump); }
    }
    public void startJumping() {
        jumpDuration = 10;
        jumpTicks = 0;
    }
    protected void defineSynchedData(SynchedEntityData.Builder entityData) {
    }
    public void customServerAiStep(ServerLevel level) {
    }
    public boolean canSpawnSprintParticle() {
        return false;
    }
    public void aiStep() {
    }
    public static AttributeSupplier.Builder createAttributes() {
        return Animal.createAnimalAttributes()
            .add(net.minecraft.world.entity.ai.attributes.Attributes.MAX_HEALTH, 3.0)
            .add(net.minecraft.world.entity.ai.attributes.Attributes.MOVEMENT_SPEED, 0.3)
            .add(net.minecraft.world.entity.ai.attributes.Attributes.ATTACK_DAMAGE, 3.0);
    }
    protected void addAdditionalSaveData(ValueOutput output) {
    }
    protected void readAdditionalSaveData(ValueInput input) {
    }
    protected SoundEvent getJumpSound() {
        return null;
    }
    protected SoundEvent getAmbientSound() {
        return net.minecraft.sounds.SoundEvents.byId("entity.rabbit.ambient");
    }
    protected SoundEvent getHurtSound(DamageSource source) {
        return net.minecraft.sounds.SoundEvents.byId("entity.rabbit.hurt");
    }
    protected SoundEvent getDeathSound() {
        return net.minecraft.sounds.SoundEvents.byId("entity.rabbit.death");
    }
    public void playAttackSound() {
    }
    public SoundSource getSoundSource() {
        return super.getSoundSource();
    }
    public Rabbit getBreedOffspring(ServerLevel level, AgeableMob partner) {
        return null;
    }
    public boolean isFood(ItemStack itemStack) {
        return live() instanceof org.bukkit.entity.Animals animals && animals.isBreedItem(org.bukkit.craftbukkit.inventory.CraftItemStack.asCraftMirror(itemStack));
    }
    public Variant getVariant() {
        return Variant.values()[live() instanceof org.bukkit.entity.Rabbit api ? api.getRabbitType().ordinal() : 0];
    }
    public <T> T get(DataComponentType<? extends T> type) {
        return super.get(type);
    }
    protected void applyImplicitComponents(DataComponentGetter components) {
        }
    protected <T> boolean applyImplicitComponent(DataComponentType<T> type, T value) {
        return false;
    }
    public SpawnGroupData finalizeSpawn(ServerLevelAccessor level, DifficultyInstance difficulty, EntitySpawnReason spawnReason, SpawnGroupData groupData) {
        return groupData;
    }
    public static boolean checkRabbitSpawnRules(EntityType<Rabbit> type, LevelAccessor level, EntitySpawnReason spawnReason, BlockPos pos, RandomSource random) {
        return level.getBlockState(pos.below()).is(net.minecraft.tags.BlockTags.RABBITS_SPAWNABLE_ON) && isBrightEnoughToSpawn(level, pos);
    }
    public void handleEntityEvent(byte id) {
    }
    public Vec3 getLeashOffset() {
        return new net.minecraft.world.phys.Vec3(0.0, 0.6F * getEyeHeight(), getBbWidth() * 0.4F);
    }
    public void baseTick() {
    }
    public static enum Variant implements StringRepresentable {
        BROWN, WHITE, BLACK, WHITE_SPLOTCHED, GOLD, SALT, EVIL;
        public static final Variant DEFAULT = null;
        public static final Codec<Variant> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.codec(net.minecraft.world.entity.animal.rabbit.Rabbit.Variant.class, "CODEC");
        public static final Codec<Variant> LEGACY_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.codec(net.minecraft.world.entity.animal.rabbit.Rabbit.Variant.class, "LEGACY_CODEC");
        public static final StreamCodec<ByteBuf, Variant> STREAM_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.streamCodec(net.minecraft.world.entity.animal.rabbit.Rabbit.Variant.class, "STREAM_CODEC");
        public String getSerializedName() {
            return name().toLowerCase(java.util.Locale.ROOT);
        }
        public int id() {
            return ordinal();
        }
        public static Variant byId(int id) {
            return values()[Math.floorMod(id, values().length)];
        }
    }
    public static class RabbitGroupData extends AgeableMob.AgeableMobGroupData {
        public final Variant variant = null;
        public RabbitGroupData(Variant variant) {
            super(false);
        }
    }
    public static class RabbitJumpControl extends JumpControl {
        public RabbitJumpControl(Rabbit rabbit) {
            super(rabbit);
        }
        public boolean wantJump() {
            throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.entity.animal.rabbit.Rabbit.RabbitJumpControl#wantJump");
        }
        public boolean canJump() {
            throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.entity.animal.rabbit.Rabbit.RabbitJumpControl#canJump");
        }
        public void setCanJump(boolean canJump) {
            throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.entity.animal.rabbit.Rabbit.RabbitJumpControl#setCanJump");
        }
        public void tick() {
        }
    }
    static class RabbitMoveControl<T extends Rabbit> extends MoveControl<T> {
        private double nextJumpSpeed;
        public RabbitMoveControl(T rabbit) {
            super(rabbit);
        }
        public void tick() {
            if (mob.onGround() && !mob.jumping) {
                mob.setSpeedModifier(0.0);
            } else if (hasWanted()) {
                mob.setSpeedModifier(nextJumpSpeed);
            }
            super.tick();
        }
        public void setWantedPosition(double x, double y, double z, double speedModifier) {
            if (mob.isInWater()) {
                speedModifier = 1.5;
            }
            super.setWantedPosition(x, y, z, speedModifier);
            if (speedModifier > 0.0) {
                nextJumpSpeed = speedModifier;
            }
        }
    }
    static class RabbitAvoidEntityGoal<T extends LivingEntity> extends AvoidEntityGoal<T> {
        private final Rabbit rabbit;
        public RabbitAvoidEntityGoal(Rabbit rabbit, Class<T> avoidClass, float maxDist, double walkSpeedModifier, double sprintSpeedModifier) {
            super(rabbit, avoidClass, maxDist, walkSpeedModifier, sprintSpeedModifier);
            this.rabbit = rabbit;
        }
        public boolean canUse() {
            return rabbit.getVariant() != Variant.EVIL && super.canUse();
        }
    }
    static class RaidGardenGoal extends MoveToBlockGoal {
        private final Rabbit rabbit;
        private boolean wantsToRaid;
        private boolean canRaid;
        public RaidGardenGoal(Rabbit rabbit) {
            super(rabbit, 0.7, 16);
            this.rabbit = rabbit;
        }
        public boolean canUse() {
            if (nextStartTick <= 0) {
                if (!Boolean.TRUE.equals(((org.bukkit.entity.Entity) rabbit.getBukkitEntity()).getWorld().getGameRuleValue(org.bukkit.GameRule.MOB_GRIEFING))) {
                    return false;
                }
                canRaid = rabbit.wantsMoreFood();
                wantsToRaid = true;
            }
            return super.canUse();
        }
        public boolean canContinueToUse() {
            return canRaid && super.canContinueToUse();
        }
        public void tick() {
            super.tick();
            rabbit.getLookControl().setLookAt(blockPos.getX() + 0.5, blockPos.getY() + 1, blockPos.getZ() + 0.5, 10.0F, rabbit.getMaxHeadXRot());
            if (!isReachedTarget()) {
                return;
            }
            BlockPos above = blockPos.above();
            BlockState state = rabbit.level().getBlockState(above);
            if (canRaid && state.is(Blocks.CARROTS) && state.getValue(net.minecraft.world.level.block.CropBlock.AGE) == 7) {
                rabbit.level().destroyBlock(above, true);
                rabbit.setMoreCarrotTicks(40);
            }
            canRaid = false;
            nextStartTick = 10;
        }
        protected boolean isValidTarget(LevelReader level, BlockPos pos) {
            if (!level.getBlockState(pos).is(Blocks.FARMLAND) || !wantsToRaid || !canRaid) {
                return false;
            }
            BlockState above = level.getBlockState(pos.above());
            return above.is(Blocks.CARROTS) && above.getValue(net.minecraft.world.level.block.CropBlock.AGE) == 7;
        }
    }
    static class RabbitPanicGoal extends PanicGoal {
        private final Rabbit rabbit;
        public RabbitPanicGoal(Rabbit rabbit, double speedModifier) {
            super(rabbit, speedModifier);
            this.rabbit = rabbit;
        }
        public void tick() {
            super.tick();
            rabbit.setSpeedModifier(speedModifier);
        }
    }
}
