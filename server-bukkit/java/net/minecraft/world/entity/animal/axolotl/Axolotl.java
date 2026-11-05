// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.animal.axolotl;
import com.mojang.serialization.Codec;
import io.netty.buffer.ByteBuf;
import net.minecraft.core.BlockPos;
import net.minecraft.core.component.DataComponentGetter;
import net.minecraft.core.component.DataComponentType;
import net.minecraft.nbt.CompoundTag;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.network.syncher.SynchedEntityData;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.sounds.SoundEvent;
import net.minecraft.util.BinaryAnimator;
import net.minecraft.util.RandomSource;
import net.minecraft.util.StringRepresentable;
import net.minecraft.world.DifficultyInstance;
import net.minecraft.world.InteractionHand;
import net.minecraft.world.InteractionResult;
import net.minecraft.world.damagesource.DamageSource;
import net.minecraft.world.entity.AgeableMob;
import net.minecraft.world.entity.AnimationState;
import net.minecraft.world.entity.Bucketable;
import net.minecraft.world.entity.EntityDimensions;
import net.minecraft.world.entity.EntitySpawnReason;
import net.minecraft.world.entity.EntityType;
import net.minecraft.world.entity.LivingEntity;
import net.minecraft.world.entity.Pose;
import net.minecraft.world.entity.SpawnGroupData;
import net.minecraft.world.entity.ai.Brain;
import net.minecraft.world.entity.ai.attributes.AttributeSupplier;
import net.minecraft.world.entity.ai.control.SmoothSwimmingLookControl;
import net.minecraft.world.entity.ai.control.SmoothSwimmingMoveControl;
import net.minecraft.world.entity.ai.navigation.PathNavigation;
import net.minecraft.world.entity.animal.Animal;
import net.minecraft.world.entity.player.Player;
import net.minecraft.world.item.ItemStack;
import net.minecraft.world.level.Level;
import net.minecraft.world.level.LevelReader;
import net.minecraft.world.level.ServerLevelAccessor;
import net.minecraft.world.level.storage.ValueInput;
import net.minecraft.world.level.storage.ValueOutput;
import net.minecraft.world.phys.Vec3;
public class Axolotl extends Animal implements Bucketable {
    public static final int TOTAL_PLAYDEAD_TIME = 200;
    public static final double PLAYER_REGEN_DETECTION_RANGE = 20.0;
    public static final int RARE_VARIANT_CHANCE = 1200;
    public static final String VARIANT_TAG = "Variant";
    public final BinaryAnimator playingDeadAnimator = null;
    public final BinaryAnimator inWaterAnimator = null;
    public final BinaryAnimator onGroundAnimator = null;
    public final BinaryAnimator movingAnimator = null;
    public final AnimationState swimAnimationState = null;
    public final AnimationState walkAnimationState = null;
    public final AnimationState walkUnderWaterAnimationState = null;
    public final AnimationState idleUnderWaterAnimationState = null;
    public final AnimationState idleUnderWaterOnGroundAnimationState = null;
    public final AnimationState idleOnGroundAnimationState = null;
    public final AnimationState playDeadAnimationState = null;
    public Axolotl(EntityType<? extends Axolotl> type, Level level) {
        super(type, level);
    }
    public float getWalkTargetValue(BlockPos pos, LevelReader level) {
        return super.getWalkTargetValue(pos, level);
    }
    protected void defineSynchedData(SynchedEntityData.Builder entityData) {
    }
    protected void addAdditionalSaveData(ValueOutput output) {
    }
    protected void readAdditionalSaveData(ValueInput input) {
    }
    public void playAmbientSound() {
    }
    public SpawnGroupData finalizeSpawn(ServerLevelAccessor level, DifficultyInstance difficulty, EntitySpawnReason spawnReason, SpawnGroupData groupData) {
        return groupData;
    }
    public void baseTick() {
    }
    protected void handleAirSupply(ServerLevel level, int preTickAirSupply) {
        }
    public void rehydrate() {
        setAirSupply(getMaxAirSupply());
    }
    public Variant getVariant() {
        return Variant.values()[live() instanceof org.bukkit.entity.Axolotl api ? api.getVariant().ordinal() : 0];
    }
    public <T> T get(DataComponentType<? extends T> type) {
        return super.get(type);
    }
    protected void applyImplicitComponents(DataComponentGetter components) {
        }
    protected <T> boolean applyImplicitComponent(DataComponentType<T> type, T value) {
        return false;
    }
    public boolean checkSpawnObstruction(LevelReader level) {
        return super.checkSpawnObstruction(level);
    }
    public void setPlayingDead(boolean playingDead) {
        if (live() instanceof org.bukkit.entity.Axolotl api) { api.setPlayingDead(playingDead); }
    }
    public boolean isPlayingDead() {
        return live() instanceof org.bukkit.entity.Axolotl api ? (boolean) api.isPlayingDead() : false;
    }
    public boolean fromBucket() {
        return live() instanceof org.bukkit.entity.Axolotl api && api.isFromBucket();
    }
    public void setFromBucket(boolean fromBucket) {
        if (live() instanceof org.bukkit.entity.Axolotl api) { api.setFromBucket(fromBucket); }
    }
    public AgeableMob getBreedOffspring(ServerLevel level, AgeableMob partner) {
        return null;
    }
    public boolean isFood(ItemStack itemStack) {
        return live() instanceof org.bukkit.entity.Animals animals && animals.isBreedItem(org.bukkit.craftbukkit.inventory.CraftItemStack.asCraftMirror(itemStack));
    }
    public boolean canBeLeashed() {
        return super.canBeLeashed();
    }
    protected void customServerAiStep(ServerLevel level) {
    }
    public static AttributeSupplier.Builder createAttributes() {
        return Animal.createAnimalAttributes()
            .add(net.minecraft.world.entity.ai.attributes.Attributes.MAX_HEALTH, 14.0)
            .add(net.minecraft.world.entity.ai.attributes.Attributes.MOVEMENT_SPEED, 1.0)
            .add(net.minecraft.world.entity.ai.attributes.Attributes.ATTACK_DAMAGE, 2.0)
            .add(net.minecraft.world.entity.ai.attributes.Attributes.STEP_HEIGHT, 1.0);
    }
    protected PathNavigation createNavigation(Level level) {
        return super.createNavigation(level);
    }
    public void playAttackSound() {
    }
    public boolean hurtServer(ServerLevel level, DamageSource source, float damage) {
        return super.hurtServer(level, source, damage);
    }
    public int getMaxHeadXRot() {
        return super.getMaxHeadXRot();
    }
    public int getMaxHeadYRot() {
        return super.getMaxHeadYRot();
    }
    public InteractionResult mobInteract(Player player, InteractionHand hand) {
        return net.minecraft.world.InteractionResult.PASS;
    }
    public void saveToBucketTag(ItemStack bucket) {
        net.minecraft.world.item.component.CustomData.update(net.minecraft.core.component.DataComponents.BUCKET_ENTITY_DATA, bucket, tag -> {
            tag.putInt("Variant", getVariant().getId());
            tag.putInt("Age", getAge());
            tag.putFloat("Health", getHealth());
        });
    }
    public void loadFromBucketTag(CompoundTag tag) {
        if (live() instanceof org.bukkit.entity.Axolotl api) {
            api.setVariant(org.bukkit.entity.Axolotl.Variant.values()[Math.floorMod(tag.getIntOr("Variant", 0), org.bukkit.entity.Axolotl.Variant.values().length)]);
        }
        setAge(tag.getIntOr("Age", 0));
        setHealth(tag.getFloatOr("Health", getHealth()));
    }
    public ItemStack getBucketItemStack() {
        return new ItemStack(net.minecraft.world.item.Items.AXOLOTL_BUCKET);
    }
    public SoundEvent getPickupSound() {
        return null;
    }
    public static void onStopAttacking(ServerLevel level, Axolotl body, LivingEntity target) {
        if (target != null && target.isDeadOrDying()) {
            body.playSound(net.minecraft.sounds.SoundEvents.byId("entity.axolotl.idle_air"), 1.0F, 1.0F);
        }
    }
    public void applySupportingEffects(Player player) {
        player.addEffect(new net.minecraft.world.effect.MobEffectInstance(net.minecraft.world.effect.MobEffects.REGENERATION, 100, 0));
    }
    protected SoundEvent getHurtSound(DamageSource source) {
        return net.minecraft.sounds.SoundEvents.byId("entity.axolotl.hurt");
    }
    protected SoundEvent getDeathSound() {
        return net.minecraft.sounds.SoundEvents.byId("entity.axolotl.death");
    }
    protected SoundEvent getAmbientSound() {
        return null;
    }
    protected SoundEvent getSwimSplashSound() {
        return null;
    }
    protected SoundEvent getSwimSound() {
        return null;
    }
    protected Brain<Axolotl> makeBrain(Brain.Packed packedBrain) {
        @SuppressWarnings("unchecked") Brain<Axolotl> made = (Brain<Axolotl>) (Brain<?>) super.makeBrain(packedBrain); return made;
    }
    public Brain<Axolotl> getBrain() {
        @SuppressWarnings("unchecked") Brain<Axolotl> made = (Brain<Axolotl>) (Brain<?>) super.getBrain(); return made;
    }
    protected void travelInWater(Vec3 input, double baseGravity, boolean isFalling, double oldY) {
        }
    protected void usePlayerItem(Player player, InteractionHand hand, ItemStack itemStack) {
        }
    public boolean removeWhenFarAway(double distSqr) {
        return super.removeWhenFarAway(distSqr);
    }
    public static boolean checkAxolotlSpawnRules(EntityType<? extends LivingEntity> type, ServerLevelAccessor level, EntitySpawnReason spawnReason, BlockPos pos, RandomSource random) {
        return level.getBlockState(pos.below()).is(net.minecraft.tags.BlockTags.AXOLOTLS_SPAWNABLE_ON);
    }
    public EntityDimensions getDefaultDimensions(Pose pose) {
        return net.minecraft.world.entity.EntityDimensions.scalable(getBbWidth(), getBbHeight());
    }
    public static enum Variant implements StringRepresentable {
        LUCY, WILD, GOLD, CYAN, BLUE;
        public static final Variant DEFAULT = null;
        public static final StreamCodec<ByteBuf, Variant> STREAM_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.streamCodec(net.minecraft.world.entity.animal.axolotl.Axolotl.Variant.class, "STREAM_CODEC");
        public static final Codec<Variant> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.codec(net.minecraft.world.entity.animal.axolotl.Axolotl.Variant.class, "CODEC");
        public static final Codec<Variant> LEGACY_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.codec(net.minecraft.world.entity.animal.axolotl.Axolotl.Variant.class, "LEGACY_CODEC");
        public int getId() {
            return ordinal();
        }
        public String getSerializedName() {
            return name().toLowerCase(java.util.Locale.ROOT);
        }
        public static Variant byId(int id) {
            return values()[Math.floorMod(id, values().length)];
        }
        public static Variant getCommonSpawnVariant(RandomSource random) {
            Variant[] common = {Variant.LUCY, Variant.WILD, Variant.GOLD, Variant.CYAN};
            return common[random.nextInt(common.length)];
        }
        public static Variant getRareSpawnVariant(RandomSource random) {
            return Variant.BLUE;
        }
    }
    static class AxolotlMoveControl<T extends Axolotl> extends SmoothSwimmingMoveControl<T> {
        public AxolotlMoveControl(T axolotl) {
            super(axolotl, 85, 10, 0.1F, 0.5F, false);
        }
        public void tick() {
        }
    }
    class AxolotlLookControl extends SmoothSwimmingLookControl {
        public AxolotlLookControl(Axolotl axolotl, int maxYRotFromCenter) {
            super(axolotl, maxYRotFromCenter);
        }
        public void tick() {
        }
    }
    public static class AxolotlGroupData extends AgeableMob.AgeableMobGroupData {
        public final Variant[] types = null;
        public AxolotlGroupData(Variant... types) {
            super(false);
        }
        public Variant getVariant(RandomSource random) {
        return types[random.nextInt(types.length)];
    }
    }
    public static enum AxolotlAnimationState {
        PLAYING_DEAD, IN_WATER, ON_GROUND, IN_AIR;
    }
}
