// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.animal.frog;
import net.minecraft.world.entity.Bucketable;
import net.minecraft.world.item.Items;
import net.minecraft.core.component.DataComponents;
import net.minecraft.world.item.component.CustomData;
import net.minecraft.nbt.CompoundTag;
import net.minecraft.network.syncher.SynchedEntityData;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.sounds.SoundEvent;
import net.minecraft.world.InteractionHand;
import net.minecraft.world.InteractionResult;
import net.minecraft.world.damagesource.DamageSource;
import net.minecraft.world.entity.EntityType;
import net.minecraft.world.entity.ai.Brain;
import net.minecraft.world.entity.ai.attributes.AttributeSupplier;
import net.minecraft.world.entity.ai.navigation.PathNavigation;
import net.minecraft.world.entity.animal.fish.AbstractFish;
import net.minecraft.world.entity.player.Player;
import net.minecraft.world.item.ItemStack;
import net.minecraft.world.level.Level;
import net.minecraft.world.level.storage.ValueInput;
import net.minecraft.world.level.storage.ValueOutput;
public class Tadpole extends AbstractFish {
    public static int ticksToBeFrog;
    public static final float HITBOX_WIDTH = 0.4F;
    public static final float HITBOX_HEIGHT = 0.3F;
    protected int ageLockParticleTimer;
    public Tadpole(EntityType<? extends AbstractFish> type, Level level) {
        super(type, level);
    }
    protected PathNavigation createNavigation(Level level) {
        return super.createNavigation(level);
    }
    protected Brain<Tadpole> makeBrain(Brain.Packed packedBrain) {
        @SuppressWarnings("unchecked") Brain<Tadpole> made = (Brain<Tadpole>) (Brain<?>) super.makeBrain(packedBrain); return made;
    }
    public Brain<Tadpole> getBrain() {
        @SuppressWarnings("unchecked") Brain<Tadpole> made = (Brain<Tadpole>) (Brain<?>) super.getBrain(); return made;
    }
    protected SoundEvent getFlopSound() {
        return null;
    }
    protected void customServerAiStep(ServerLevel level) {
    }
    public static AttributeSupplier.Builder createAttributes() {
        return net.minecraft.world.entity.Mob.createMobAttributes();
    }
    public void aiStep() {
    }
    protected void addAdditionalSaveData(ValueOutput output) {
    }
    protected void readAdditionalSaveData(ValueInput input) {
    }
    protected void defineSynchedData(SynchedEntityData.Builder entityData) {
    }
    protected void setAgeLocked(boolean locked) {
        }
    public boolean isAgeLocked() {
        return live() instanceof org.bukkit.entity.Tadpole api && api.getAgeLock();
    }
    protected SoundEvent getAmbientSound() {
        return null;
    }
    protected SoundEvent getHurtSound(DamageSource source) {
        return null;
    }
    protected SoundEvent getDeathSound() {
        return null;
    }
    public InteractionResult mobInteract(Player player, InteractionHand hand) {
        return net.minecraft.world.InteractionResult.PASS;
    }
    public boolean fromBucket() {
        return true;
    }
    public void setFromBucket(boolean fromBucket) {
        if (live() instanceof org.bukkit.entity.Tadpole api) { api.setFromBucket(fromBucket); }
    }
    public void saveToBucketTag(ItemStack bucket) {
        Bucketable.saveDefaultDataToBucketTag(this, bucket);
        CustomData.update(DataComponents.BUCKET_ENTITY_DATA, bucket, tag -> {
            tag.putInt("Age", live() instanceof org.bukkit.entity.Tadpole api ? api.getAge() : 0);
            tag.putBoolean("AgeLocked", isAgeLocked());
        });
    }
    public void loadFromBucketTag(CompoundTag tag) {
        Bucketable.loadDefaultDataFromBucketTag(this, tag);
        tag.getInt("Age").ifPresent(age -> {
            if (live() instanceof org.bukkit.entity.Tadpole api) {
                api.setAge(age);
            }
        });
        setAgeLocked(tag.getBooleanOr("AgeLocked", false));
    }
    public ItemStack getBucketItemStack() {
        return new ItemStack(Items.TADPOLE_BUCKET);
    }
    public SoundEvent getPickupSound() {
        return null;
    }
    public boolean shouldDropExperience() {
        return false;

    }
}
