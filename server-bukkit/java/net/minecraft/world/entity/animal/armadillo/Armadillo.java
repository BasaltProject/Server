// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.animal.armadillo;
import io.netty.buffer.ByteBuf;
import net.minecraft.core.BlockPos;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.network.syncher.EntityDataAccessor;
import net.minecraft.network.syncher.SynchedEntityData;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.sounds.SoundEvent;
import net.minecraft.util.RandomSource;
import net.minecraft.util.StringRepresentable;
import net.minecraft.world.InteractionHand;
import net.minecraft.world.InteractionResult;
import net.minecraft.world.damagesource.DamageSource;
import net.minecraft.world.entity.AgeableMob;
import net.minecraft.world.entity.AnimationState;
import net.minecraft.world.entity.Entity;
import net.minecraft.world.entity.EntityDimensions;
import net.minecraft.world.entity.EntitySpawnReason;
import net.minecraft.world.entity.EntityType;
import net.minecraft.world.entity.LivingEntity;
import net.minecraft.world.entity.Pose;
import net.minecraft.world.entity.ai.Brain;
import net.minecraft.world.entity.ai.attributes.AttributeSupplier;
import net.minecraft.world.entity.ai.control.BodyRotationControl;
import net.minecraft.world.entity.animal.Animal;
import net.minecraft.world.entity.player.Player;
import net.minecraft.world.item.ItemStack;
import net.minecraft.world.level.Level;
import net.minecraft.world.level.LevelAccessor;
import net.minecraft.world.level.block.state.BlockState;
import net.minecraft.world.level.storage.ValueInput;
import net.minecraft.world.level.storage.ValueOutput;
public class Armadillo extends Animal {
    public static final float BABY_SCALE = 0.6F;
    public static final float MAX_HEAD_ROTATION_EXTENT = 32.5F;
    public static final int SCARE_CHECK_INTERVAL = 80;
    public final AnimationState rollOutAnimationState = null;
    public final AnimationState rollUpAnimationState = null;
    public final AnimationState peekAnimationState = null;
    public Armadillo(EntityType<? extends Animal> type, Level level) {
        super(type, level);
    }
    public AgeableMob getBreedOffspring(ServerLevel level, AgeableMob partner) {
        return null;
    }
    public static AttributeSupplier.Builder createAttributes() {
        return Animal.createAnimalAttributes()
            .add(net.minecraft.world.entity.ai.attributes.Attributes.MAX_HEALTH, 12.0)
            .add(net.minecraft.world.entity.ai.attributes.Attributes.MOVEMENT_SPEED, 0.14);
    }
    protected void defineSynchedData(SynchedEntityData.Builder entityData) {
    }
    public boolean isScared() {
        return getState() == ArmadilloState.SCARED;
    }
    public boolean shouldHideInShell() {
        return getState() == ArmadilloState.SCARED || getState() == ArmadilloState.ROLLING;
    }
    public boolean shouldSwitchToScaredState() {
        return getState() == ArmadilloState.IDLE && getLastHurtByMob() != null;
    }
    public ArmadilloState getState() {
        return ArmadilloState.values()[live() instanceof org.bukkit.entity.Armadillo api ? api.getState().ordinal() : 0];
    }
    public void switchToState(ArmadilloState state) {
        if (live() instanceof org.bukkit.entity.Armadillo api) {
            if (state == ArmadilloState.SCARED || state == ArmadilloState.ROLLING) { api.rollUp(); } else if (state == ArmadilloState.IDLE) { api.rollOut(); }
        }
    }
    public void onSyncedDataUpdated(EntityDataAccessor<?> accessor) {
    }
    protected Brain<Armadillo> makeBrain(Brain.Packed packedBrain) {
        @SuppressWarnings("unchecked") Brain<Armadillo> made = (Brain<Armadillo>) (Brain<?>) super.makeBrain(packedBrain); return made;
    }
    public Brain<Armadillo> getBrain() {
        @SuppressWarnings("unchecked") Brain<Armadillo> made = (Brain<Armadillo>) (Brain<?>) super.getBrain(); return made;
    }
    protected void customServerAiStep(ServerLevel level) {
    }
    public void tick() {
    }
    public EntityDimensions getDefaultDimensions(Pose pose) {
        return net.minecraft.world.entity.EntityDimensions.scalable(getBbWidth(), getBbHeight());
    }
    public void handleEntityEvent(byte id) {
    }
    public boolean isFood(ItemStack itemStack) {
        return live() instanceof org.bukkit.entity.Animals animals && animals.isBreedItem(org.bukkit.craftbukkit.inventory.CraftItemStack.asCraftMirror(itemStack));
    }
    public static boolean checkArmadilloSpawnRules(EntityType<Armadillo> type, LevelAccessor level, EntitySpawnReason spawnReason, BlockPos pos, RandomSource random) {
        return level.getBlockState(pos.below()).is(net.minecraft.tags.BlockTags.ARMADILLO_SPAWNABLE_ON) && isBrightEnoughToSpawn(level, pos);
    }
    public boolean isScaredBy(LivingEntity livingEntity) {
        return livingEntity.isAlive() && !(livingEntity instanceof Armadillo) && livingEntity.distanceToSqr(this) < 49.0;
    }
    protected void addAdditionalSaveData(ValueOutput output) {
    }
    protected void readAdditionalSaveData(ValueInput input) {
    }
    public void rollUp() {
        if (live() instanceof org.bukkit.entity.Armadillo api) { api.rollUp(); }
    }
    public void rollOut() {
        if (live() instanceof org.bukkit.entity.Armadillo api) { api.rollOut(); }
    }
    public boolean hurtServer(ServerLevel level, DamageSource source, float damage) {
        return super.hurtServer(level, source, damage);
    }
    protected void actuallyHurt(ServerLevel level, DamageSource source, float dmg) {
        }
    public InteractionResult mobInteract(Player player, InteractionHand hand) {
        return net.minecraft.world.InteractionResult.PASS;
    }
    public boolean brushOffScute(Entity interactingEntity, ItemStack tool) {
        if (live() == null || isBaby()) {
            return false;
        }
        spawnAtLocation(null, new ItemStack(net.minecraft.world.item.Items.ARMADILLO_SCUTE));
        return true;
    }
    public boolean canStayRolledUp() {
        return !isInWater() && !isLeashed() && !isPassenger();
    }
    protected SoundEvent getAmbientSound() {
        return net.minecraft.sounds.SoundEvents.byId("entity.armadillo.ambient");
    }
    protected void playEatingSound() {
        }
    protected SoundEvent getDeathSound() {
        return net.minecraft.sounds.SoundEvents.byId("entity.armadillo.death");
    }
    protected SoundEvent getHurtSound(DamageSource source) {
        return net.minecraft.sounds.SoundEvents.byId("entity.armadillo.hurt");
    }
    protected void playStepSound(BlockPos pos, BlockState blockState) {
        playSound(net.minecraft.sounds.SoundEvents.byId("entity.armadillo.step"), 0.15F, 1.0F);
    }
    public int getMaxHeadYRot() {
        return super.getMaxHeadYRot();
    }
    public static enum ArmadilloState implements StringRepresentable {
        IDLE, ROLLING, SCARED, UNROLLING;
        public static final StreamCodec<ByteBuf, ArmadilloState> STREAM_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.streamCodec(net.minecraft.world.entity.animal.armadillo.Armadillo.ArmadilloState.class, "STREAM_CODEC");
        public String getSerializedName() {
            return name().toLowerCase(java.util.Locale.ROOT);
        }
        public boolean shouldHideInShell(long ticksInState) {
            return this == ROLLING ? ticksInState > 5L : this == SCARED && ticksInState > 75L;
        }
        public boolean isThreatened() {
            return this != IDLE;
        }
        public int animationDuration() {
            return this == ROLLING ? 10 : this == UNROLLING ? 30 : this == SCARED ? 50 : 0;
        }
    }
}
