// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.animal.goat;
import net.minecraft.core.BlockPos;
import net.minecraft.network.syncher.SynchedEntityData;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.sounds.SoundEvent;
import net.minecraft.util.RandomSource;
import net.minecraft.world.DifficultyInstance;
import net.minecraft.world.InteractionHand;
import net.minecraft.world.InteractionResult;
import net.minecraft.world.damagesource.DamageSource;
import net.minecraft.world.entity.AgeableMob;
import net.minecraft.world.entity.EntityDimensions;
import net.minecraft.world.entity.EntitySpawnReason;
import net.minecraft.world.entity.EntityType;
import net.minecraft.world.entity.Pose;
import net.minecraft.world.entity.SpawnGroupData;
import net.minecraft.world.entity.ai.Brain;
import net.minecraft.world.entity.ai.attributes.AttributeSupplier;
import net.minecraft.world.entity.animal.Animal;
import net.minecraft.world.entity.player.Player;
import net.minecraft.world.item.ItemStack;
import net.minecraft.world.level.Level;
import net.minecraft.world.level.LevelAccessor;
import net.minecraft.world.level.ServerLevelAccessor;
import net.minecraft.world.level.block.state.BlockState;
import net.minecraft.world.level.storage.ValueInput;
import net.minecraft.world.level.storage.ValueOutput;
public class Goat extends Animal {
    public static final float LONG_JUMPING_DIMENSION_SCALE_FACTOR = 0.7F;
    public static final float BABY_DEFAULT_X_HEAD_ROT = 22.5F;
    public static final float MAX_ADDED_RAMMING_X_HEAD_ROT = 30.0F;
    public static final int GOAT_FALL_DAMAGE_REDUCTION = 10;
    public static final double GOAT_SCREAMING_CHANCE = 0.02;
    public static final double UNIHORN_CHANCE = 0.1;
    public Goat(EntityType<? extends Goat> type, Level level) {
        super(type, level);
    }
    public ItemStack createHorn() {
        return new ItemStack(net.minecraft.world.item.Items.GOAT_HORN);
    }
    protected Brain<Goat> makeBrain(Brain.Packed packedBrain) {
        @SuppressWarnings("unchecked") Brain<Goat> made = (Brain<Goat>) (Brain<?>) super.makeBrain(packedBrain); return made;
    }
    public static AttributeSupplier.Builder createAttributes() {
        return Animal.createAnimalAttributes()
            .add(net.minecraft.world.entity.ai.attributes.Attributes.MAX_HEALTH, 10.0)
            .add(net.minecraft.world.entity.ai.attributes.Attributes.MOVEMENT_SPEED, 0.2)
            .add(net.minecraft.world.entity.ai.attributes.Attributes.ATTACK_DAMAGE, 2.0);
    }
    protected void ageBoundaryReached() {
        }
    protected int calculateFallDamage(double fallDistance, float damageModifier) {
        return 0;
    }
    protected SoundEvent getAmbientSound() {
        return net.minecraft.sounds.SoundEvents.byId("entity.goat.ambient");
    }
    protected SoundEvent getHurtSound(DamageSource source) {
        return net.minecraft.sounds.SoundEvents.byId("entity.goat.hurt");
    }
    protected SoundEvent getDeathSound() {
        return net.minecraft.sounds.SoundEvents.byId("entity.goat.death");
    }
    protected void playStepSound(BlockPos pos, BlockState blockState) {
        playSound(net.minecraft.sounds.SoundEvents.byId("entity.goat.step"), 0.15F, 1.0F);
    }
    protected SoundEvent getMilkingSound() {
        return null;
    }
    public Goat getBreedOffspring(ServerLevel level, AgeableMob partner) {
        return null;
    }
    public Brain<Goat> getBrain() {
        @SuppressWarnings("unchecked") Brain<Goat> made = (Brain<Goat>) (Brain<?>) super.getBrain(); return made;
    }
    protected void customServerAiStep(ServerLevel level) {
    }
    public int getMaxHeadYRot() {
        return super.getMaxHeadYRot();
    }
    protected void playEatingSound() {
        }
    public boolean isFood(ItemStack itemStack) {
        return live() instanceof org.bukkit.entity.Animals animals && animals.isBreedItem(org.bukkit.craftbukkit.inventory.CraftItemStack.asCraftMirror(itemStack));
    }
    public InteractionResult mobInteract(Player player, InteractionHand hand) {
        return net.minecraft.world.InteractionResult.PASS;
    }
    public SpawnGroupData finalizeSpawn(ServerLevelAccessor level, DifficultyInstance difficulty, EntitySpawnReason spawnReason, SpawnGroupData groupData) {
        return groupData;
    }
    public EntityDimensions getDefaultDimensions(Pose pose) {
        return net.minecraft.world.entity.EntityDimensions.scalable(getBbWidth(), getBbHeight());
    }
    protected void addAdditionalSaveData(ValueOutput output) {
    }
    protected void readAdditionalSaveData(ValueInput input) {
    }
    public void handleEntityEvent(byte id) {
    }
    public void aiStep() {
    }
    protected void defineSynchedData(SynchedEntityData.Builder entityData) {
    }
    public boolean hasLeftHorn() {
        return live() instanceof org.bukkit.entity.Goat api ? (boolean) api.hasLeftHorn() : false;
    }
    public boolean hasRightHorn() {
        return live() instanceof org.bukkit.entity.Goat api ? (boolean) api.hasRightHorn() : false;
    }
    public boolean dropHorn() {
        if (!(live() instanceof org.bukkit.entity.Goat api) || !api.hasLeftHorn() && !api.hasRightHorn()) {
            return false;
        }
        boolean left = api.hasLeftHorn() && (!api.hasRightHorn() || random.nextBoolean());
        if (left) {
            api.setLeftHorn(false);
        } else {
            api.setRightHorn(false);
        }
        net.minecraft.world.phys.Vec3 at = position().add(0.0, getEyeHeight(), 0.0);
        level().addFreshEntity(new net.minecraft.world.entity.item.ItemEntity(level(), at.x, at.y, at.z, createHorn()));
        return true;
    }
    public boolean isScreamingGoat() {
        return live() instanceof org.bukkit.entity.Goat api && api.isScreaming();
    }
    public void setScreamingGoat(boolean isScreamingGoat) {
        if (live() instanceof org.bukkit.entity.Goat api) {
            api.setScreaming(isScreamingGoat);
        }
    }
    public float getRammingXHeadRot() {
        return 0.0F;
    }
    public static boolean checkGoatSpawnRules(EntityType<? extends Animal> type, LevelAccessor level, EntitySpawnReason spawnReason, BlockPos pos, RandomSource random) {
        return level.getBlockState(pos.below()).is(net.minecraft.tags.BlockTags.GOATS_SPAWNABLE_ON) && isBrightEnoughToSpawn(level, pos);
    }
}
