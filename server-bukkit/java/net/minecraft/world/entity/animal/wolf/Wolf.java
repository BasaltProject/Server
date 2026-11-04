// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.animal.wolf;
import net.minecraft.core.BlockPos;
import net.minecraft.core.Holder;
import net.minecraft.core.component.DataComponentGetter;
import net.minecraft.core.component.DataComponentType;
import net.minecraft.network.syncher.SynchedEntityData;
import net.minecraft.resources.Identifier;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.sounds.SoundEvent;
import net.minecraft.util.RandomSource;
import net.minecraft.world.DifficultyInstance;
import net.minecraft.world.InteractionHand;
import net.minecraft.world.InteractionResult;
import net.minecraft.world.damagesource.DamageSource;
import net.minecraft.world.entity.AgeableMob;
import net.minecraft.world.entity.EntityDimensions;
import net.minecraft.world.entity.EntityReference;
import net.minecraft.world.entity.EntitySpawnReason;
import net.minecraft.world.entity.EntityType;
import net.minecraft.world.entity.LivingEntity;
import net.minecraft.world.entity.NeutralMob;
import net.minecraft.world.entity.Pose;
import net.minecraft.world.entity.SpawnGroupData;
import net.minecraft.world.entity.TamableAnimal;
import net.minecraft.world.entity.ai.attributes.AttributeSupplier;
import net.minecraft.world.entity.ai.goal.AvoidEntityGoal;
import net.minecraft.world.entity.ai.targeting.TargetingConditions;
import net.minecraft.world.entity.animal.Animal;
import net.minecraft.world.entity.player.Player;
import net.minecraft.world.item.DyeColor;
import net.minecraft.world.item.ItemStack;
import net.minecraft.world.level.Level;
import net.minecraft.world.level.LevelAccessor;
import net.minecraft.world.level.ServerLevelAccessor;
import net.minecraft.world.level.block.state.BlockState;
import net.minecraft.world.level.storage.ValueInput;
import net.minecraft.world.level.storage.ValueOutput;
import net.minecraft.world.phys.Vec3;
public class Wolf extends TamableAnimal implements NeutralMob {
    private long angerEndTime = -1L;
    private net.minecraft.world.entity.EntityReference<net.minecraft.world.entity.LivingEntity> angerTarget;
    public static final TargetingConditions.Selector PREY_SELECTOR = null;
    public static final float DEFAULT_TAIL_ANGLE = 0;
    public Wolf(EntityType<? extends Wolf> type, Level level) {
        super(type, level);
    }
    protected void registerGoals() {
        goalSelector.addGoal(1, new net.minecraft.world.entity.ai.goal.FloatGoal(this));
        goalSelector.addGoal(4, new net.minecraft.world.entity.ai.goal.LeapAtTargetGoal(this, 0.4F));
        goalSelector.addGoal(5, new net.minecraft.world.entity.ai.goal.MeleeAttackGoal(this, 1.0, true));
        goalSelector.addGoal(7, new net.minecraft.world.entity.ai.goal.BreedGoal(this, 1.0));
        goalSelector.addGoal(8, new net.minecraft.world.entity.ai.goal.WaterAvoidingRandomStrollGoal(this, 1.0));
        goalSelector.addGoal(10, new net.minecraft.world.entity.ai.goal.LookAtPlayerGoal(this, net.minecraft.world.entity.player.Player.class, 8.0F));
        goalSelector.addGoal(10, new net.minecraft.world.entity.ai.goal.RandomLookAroundGoal(this));
        targetSelector.addGoal(3, new net.minecraft.world.entity.ai.goal.target.HurtByTargetGoal(this));
        targetSelector.addGoal(7, new net.minecraft.world.entity.ai.goal.target.NearestAttackableTargetGoal<>(this, net.minecraft.world.entity.monster.skeleton.AbstractSkeleton.class, false));
    }
    public Identifier getTexture() {
        String name = getVariant() == null ? "pale" : getVariant().unwrapKey().map(key -> key.identifier().getPath()).orElse("pale");
        String base = name.equals("pale") ? "wolf" : "wolf_" + name;
        String state = isTame() ? "_tame" : isAngry() ? "_angry" : "";
        return Identifier.withDefaultNamespace("textures/entity/wolf/" + (isBaby() ? "baby_" : "") + base + state + ".png");
    }
    protected void applyImplicitComponents(DataComponentGetter components) {
        }
    protected <T> boolean applyImplicitComponent(DataComponentType<T> type, T value) {
        return false;
    }
    public static AttributeSupplier.Builder createAttributes() {
        return Animal.createAnimalAttributes()
            .add(net.minecraft.world.entity.ai.attributes.Attributes.MOVEMENT_SPEED, 0.3)
            .add(net.minecraft.world.entity.ai.attributes.Attributes.MAX_HEALTH, 8.0)
            .add(net.minecraft.world.entity.ai.attributes.Attributes.ATTACK_DAMAGE, 4.0);
    }
    protected void defineSynchedData(SynchedEntityData.Builder entityData) {
    }
    protected void playStepSound(BlockPos pos, BlockState blockState) {
        playSound(net.minecraft.sounds.SoundEvents.byId("entity.wolf.step"), 0.15F, 1.0F);
    }
    protected void addAdditionalSaveData(ValueOutput output) {
    }
    protected void readAdditionalSaveData(ValueInput input) {
    }
    public SpawnGroupData finalizeSpawn(ServerLevelAccessor level, DifficultyInstance difficulty, EntitySpawnReason spawnReason, SpawnGroupData groupData) {
        return groupData;
    }
    protected SoundEvent getAmbientSound() {
        return net.minecraft.sounds.SoundEvents.byId("entity.wolf.ambient");
    }
    protected SoundEvent getHurtSound(DamageSource source) {
        return net.minecraft.sounds.SoundEvents.byId("entity.wolf.hurt");
    }
    protected SoundEvent getDeathSound() {
        return net.minecraft.sounds.SoundEvents.byId("entity.wolf.death");
    }
    protected float getSoundVolume() {
        return live() instanceof org.bukkit.entity.Wolf api ? (float) api.getSoundVolume() : 0;
    }
    public void aiStep() {
    }
    public void tick() {
    }
    public float getWetShade(float a) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.entity.animal.wolf.Wolf#getWetShade");
    }
    public float getShakeAnim(float a) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.entity.animal.wolf.Wolf#getShakeAnim");
    }
    public float getHeadRollAngle(float a) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.entity.animal.wolf.Wolf#getHeadRollAngle");
    }
    public int getMaxHeadXRot() {
        return super.getMaxHeadXRot();
    }
    public boolean hurtServer(ServerLevel level, DamageSource source, float damage) {
        return super.hurtServer(level, source, damage);
    }
    protected void actuallyHurt(ServerLevel level, DamageSource source, float damage) {
        }
    protected void applyTamingSideEffects() {
        }
    protected void hurtArmor(DamageSource damageSource, float damage) {
        }
    protected boolean canShearEquipment(Player player) {
        return false;
    }
    public InteractionResult mobInteract(Player player, InteractionHand hand) {
        return net.minecraft.world.InteractionResult.PASS;
    }
    public void handleEntityEvent(byte id) {
    }
    public float getTailAngle() {
        return live() instanceof org.bukkit.entity.Wolf api ? (float) api.getTailAngle() : 0;
    }
    public boolean isFood(ItemStack itemStack) {
        return live() instanceof org.bukkit.entity.Animals animals && animals.isBreedItem(org.bukkit.craftbukkit.inventory.CraftItemStack.asCraftMirror(itemStack));
    }
    public long getPersistentAngerEndTime() {
        return angerEndTime;
    }
    public void setPersistentAngerEndTime(long endTime) {
        this.angerEndTime = endTime;
    }
    public void startPersistentAngerTimer() {
        setTimeToRemainAngry(400 + random.nextInt(381));
    }
    public EntityReference<LivingEntity> getPersistentAngerTarget() {
        return angerTarget;
    }
    public void setPersistentAngerTarget(EntityReference<LivingEntity> persistentAngerTarget) {
        this.angerTarget = persistentAngerTarget;
    }
    public DyeColor getCollarColor() {
        return live() instanceof org.bukkit.entity.Wolf api ? DyeColor.byId(api.getCollarColor().ordinal()) : DyeColor.RED;
    }
    public EntityDimensions getDefaultDimensions(Pose pose) {
        return net.minecraft.world.entity.EntityDimensions.scalable(getBbWidth(), getBbHeight());
    }
    public Wolf getBreedOffspring(ServerLevel level, AgeableMob partner) {
        return null;
    }
    public void setIsInterested(boolean value) {
        if (live() instanceof org.bukkit.entity.Wolf api) {
            api.setInterested(value);
        }
    }
    public boolean canMate(Animal partner) {
        return partner != this && partner.getClass() == getClass() && live() instanceof org.bukkit.entity.Animals selfApi && selfApi.isLoveMode() && partner.live() instanceof org.bukkit.entity.Animals otherApi && otherApi.isLoveMode();
    }
    public boolean isInterested() {
        return live() instanceof org.bukkit.entity.Wolf api ? (boolean) api.isInterested() : false;
    }
    public boolean canBeLeashed() {
        return super.canBeLeashed();
    }
    public Vec3 getLeashOffset() {
        return new Vec3(0.0, 0.6F * getEyeHeight(), getBbWidth() * 0.4F);
    }
    public static boolean checkWolfSpawnRules(EntityType<Wolf> type, LevelAccessor level, EntitySpawnReason spawnReason, BlockPos pos, RandomSource random) {
        return level.getBlockState(pos.below()).is(net.minecraft.tags.BlockTags.WOLVES_SPAWNABLE_ON) && isBrightEnoughToSpawn(level, pos);
    }
    public Holder<WolfVariant> getVariant() {
        return live() instanceof org.bukkit.entity.Wolf api
            ? org.basaltmc.bukkit.facade.VariantHolders.of(net.minecraft.core.registries.Registries.WOLF_VARIANT, WolfVariant.class, api.getVariant()) : null;
    }
    public static class WolfPackData extends AgeableMob.AgeableMobGroupData {
        public final Holder<WolfVariant> type = null;
        public WolfPackData(Holder<WolfVariant> type) {
            super(false);
        }
    }
    class WolfAvoidEntityGoal<T extends LivingEntity> extends AvoidEntityGoal<T> {
        public WolfAvoidEntityGoal(Wolf wolf, Class<T> avoidClass, float maxDist, double walkSpeedModifier, double sprintSpeedModifier) {
            super(wolf, avoidClass, maxDist, walkSpeedModifier, sprintSpeedModifier);
        }
    }
}
