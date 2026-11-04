// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.animal.fish;
import net.minecraft.network.syncher.EntityDataAccessor;
import net.minecraft.network.syncher.SynchedEntityData;
import net.minecraft.sounds.SoundEvent;
import net.minecraft.world.damagesource.DamageSource;
import net.minecraft.world.entity.EntityDimensions;
import net.minecraft.world.entity.EntityType;
import net.minecraft.world.entity.Pose;
import net.minecraft.world.entity.ai.goal.Goal;
import net.minecraft.world.entity.player.Player;
import net.minecraft.world.item.ItemStack;
import net.minecraft.world.level.Level;
import net.minecraft.world.level.storage.ValueInput;
import net.minecraft.world.level.storage.ValueOutput;
public class Pufferfish extends AbstractFish {
    public static final int STATE_SMALL = 0;
    public static final int STATE_MID = 1;
    public static final int STATE_FULL = 2;
    public Pufferfish(EntityType<? extends Pufferfish> type, Level level) {
        super(type, level);
    }
    protected void defineSynchedData(SynchedEntityData.Builder entityData) {
    }
    public int getPuffState() {
        return live() instanceof org.bukkit.entity.PufferFish api ? api.getPuffState() : 0;
    }
    public void setPuffState(int state) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.entity.animal.fish.Pufferfish#setPuffState");
    }
    public void onSyncedDataUpdated(EntityDataAccessor<?> accessor) {
    }
    protected void addAdditionalSaveData(ValueOutput output) {
    }
    protected void readAdditionalSaveData(ValueInput input) {
    }
    public ItemStack getBucketItemStack() {
        return new ItemStack(net.minecraft.world.item.Items.PUFFERFISH_BUCKET);
    }
    protected void registerGoals() {
    }
    public void tick() {
    }
    public void aiStep() {
    }
    protected SoundEvent getDeathSound() {
        return net.minecraft.sounds.SoundEvents.byId("entity.puffer_fish.death");
    }
    protected SoundEvent getHurtSound(DamageSource source) {
        return net.minecraft.sounds.SoundEvents.byId("entity.puffer_fish.hurt");
    }
    protected SoundEvent getFlopSound() {
        return null;
    }
    public EntityDimensions getDefaultDimensions(Pose pose) {
        return net.minecraft.world.entity.EntityDimensions.scalable(getBbWidth(), getBbHeight());
    }
    static class PufferfishPuffGoal extends Goal {
        private final Pufferfish fish;
        public PufferfishPuffGoal(Pufferfish fish) {
            this.fish = fish;
        }
        public boolean canUse() {
            return !fish.level().getEntitiesOfClass(net.minecraft.world.entity.LivingEntity.class, fish.getBoundingBox().inflate(2.0),
                other -> other != fish && other.isAlive() && !(other instanceof net.minecraft.world.entity.animal.fish.AbstractFish)
                    && !(other instanceof net.minecraft.world.entity.player.Player p && (p.isCreative() || p.isSpectator()))).isEmpty();
        }
        public void start() {
            if (fish.live() instanceof org.bukkit.entity.PufferFish api) {
                api.setPuffState(2);
            }
        }
        public void stop() {
            if (fish.live() instanceof org.bukkit.entity.PufferFish api) {
                api.setPuffState(0);
            }
        }
    }
}
