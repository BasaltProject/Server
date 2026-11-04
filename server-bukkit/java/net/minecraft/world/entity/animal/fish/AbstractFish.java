// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.animal.fish;
import net.minecraft.core.BlockPos;
import net.minecraft.nbt.CompoundTag;
import net.minecraft.network.syncher.SynchedEntityData;
import net.minecraft.sounds.SoundEvent;
import net.minecraft.world.InteractionHand;
import net.minecraft.world.InteractionResult;
import net.minecraft.world.entity.Bucketable;
import net.minecraft.world.entity.EntityType;
import net.minecraft.world.entity.ai.attributes.AttributeSupplier;
import net.minecraft.world.entity.ai.control.MoveControl;
import net.minecraft.world.entity.ai.goal.RandomSwimmingGoal;
import net.minecraft.world.entity.ai.navigation.PathNavigation;
import net.minecraft.world.entity.player.Player;
import net.minecraft.world.item.ItemStack;
import net.minecraft.world.level.Level;
import net.minecraft.world.level.block.state.BlockState;
import net.minecraft.world.level.storage.ValueInput;
import net.minecraft.world.level.storage.ValueOutput;
import net.minecraft.world.phys.Vec3;
public abstract class AbstractFish extends WaterAnimal implements Bucketable {
    public AbstractFish(EntityType<? extends AbstractFish> type, Level level) {
        super(type, level);
    }
    public static AttributeSupplier.Builder createAttributes() {
        return net.minecraft.world.entity.Mob.createMobAttributes()
            .add(net.minecraft.world.entity.ai.attributes.Attributes.MAX_HEALTH, 3.0);
    }
    public boolean removeWhenFarAway(double distSqr) {
        return super.removeWhenFarAway(distSqr);
    }
    protected void defineSynchedData(SynchedEntityData.Builder entityData) {
    }
    public boolean fromBucket() {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.entity.animal.fish.AbstractFish#fromBucket");
    }
    public void setFromBucket(boolean fromBucket) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.entity.animal.fish.AbstractFish#setFromBucket");
    }
    protected void addAdditionalSaveData(ValueOutput output) {
    }
    protected void readAdditionalSaveData(ValueInput input) {
    }
    protected void registerGoals() {
    }
    protected PathNavigation createNavigation(Level level) {
        return super.createNavigation(level);
    }
    protected void travelInWater(Vec3 input, double baseGravity, boolean isFalling, double oldY) {
        }
    public void aiStep() {
    }
    protected InteractionResult mobInteract(Player player, InteractionHand hand) {
        return net.minecraft.world.InteractionResult.PASS;
    }
    public void saveToBucketTag(ItemStack bucket) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.entity.animal.fish.AbstractFish#saveToBucketTag");
    }
    public void loadFromBucketTag(CompoundTag tag) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.entity.animal.fish.AbstractFish#loadFromBucketTag");
    }
    public SoundEvent getPickupSound() {
        return null;
    }
    protected boolean canRandomSwim() {
        return false;
    }
    protected abstract SoundEvent getFlopSound();
    protected SoundEvent getSwimSound() {
        return null;
    }
    protected void playStepSound(BlockPos pos, BlockState blockState) {
    }
    static class FishSwimGoal extends RandomSwimmingGoal {
        public FishSwimGoal(AbstractFish fish) {
            super(fish, (double) 0, (int) 0);
        }
    }
    static class FishMoveControl<T extends AbstractFish> extends MoveControl<T> {
        public FishMoveControl(T fish) {
            super(fish);
        }
        public void tick() {
        }
    }
}
