// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.animal.nautilus;
import net.minecraft.core.Holder;
import net.minecraft.core.component.DataComponentGetter;
import net.minecraft.core.component.DataComponentType;
import net.minecraft.network.syncher.SynchedEntityData;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.sounds.SoundEvent;
import net.minecraft.world.DifficultyInstance;
import net.minecraft.world.damagesource.DamageSource;
import net.minecraft.world.entity.AgeableMob;
import net.minecraft.world.entity.EntitySpawnReason;
import net.minecraft.world.entity.EntityType;
import net.minecraft.world.entity.EquipmentSlot;
import net.minecraft.world.entity.SpawnGroupData;
import net.minecraft.world.entity.ai.Brain;
import net.minecraft.world.entity.ai.attributes.AttributeSupplier;
import net.minecraft.world.level.Level;
import net.minecraft.world.level.ServerLevelAccessor;
import net.minecraft.world.level.storage.ValueInput;
import net.minecraft.world.level.storage.ValueOutput;
public class ZombieNautilus extends AbstractNautilus {
    public ZombieNautilus(EntityType<? extends ZombieNautilus> type, Level level) {
        super(type, level);
    }
    public static AttributeSupplier.Builder createAttributes() {
        return net.minecraft.world.entity.Mob.createMobAttributes();
    }
    public ZombieNautilus getBreedOffspring(ServerLevel level, AgeableMob partner) {
        return null;
    }
    protected Brain<ZombieNautilus> makeBrain(Brain.Packed packedBrain) {
        @SuppressWarnings("unchecked") Brain<ZombieNautilus> made = (Brain<ZombieNautilus>) (Brain<?>) super.makeBrain(packedBrain); return made;
    }
    public Brain<ZombieNautilus> getBrain() {
        @SuppressWarnings("unchecked") Brain<ZombieNautilus> made = (Brain<ZombieNautilus>) (Brain<?>) super.getBrain(); return made;
    }
    protected void customServerAiStep(ServerLevel level) {
    }
    protected SoundEvent getAmbientSound() {
        return net.minecraft.sounds.SoundEvents.ZOMBIE_NAUTILUS_AMBIENT;
    }
    protected SoundEvent getHurtSound(DamageSource source) {
        return net.minecraft.sounds.SoundEvents.ZOMBIE_NAUTILUS_HURT;
    }
    protected SoundEvent getDeathSound() {
        return net.minecraft.sounds.SoundEvents.ZOMBIE_NAUTILUS_DEATH;
    }
    protected SoundEvent getDashSound() {
        return null;
    }
    protected SoundEvent getDashReadySound() {
        return null;
    }
    protected void playEatingSound() {
        }
    protected SoundEvent getSwimSound() {
        return null;
    }
    protected void defineSynchedData(SynchedEntityData.Builder entityData) {
    }
    protected void readAdditionalSaveData(ValueInput input) {
    }
    protected void addAdditionalSaveData(ValueOutput output) {
    }
    public void setVariant(Holder<ZombieNautilusVariant> variant) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.entity.animal.nautilus.ZombieNautilus#setVariant");
    }
    public Holder<ZombieNautilusVariant> getVariant() {
        return live() instanceof org.bukkit.entity.ZombieNautilus api
            ? org.basaltmc.bukkit.facade.VariantHolders.of(net.minecraft.core.registries.Registries.ZOMBIE_NAUTILUS_VARIANT, net.minecraft.world.entity.animal.nautilus.ZombieNautilusVariant.class, api.getVariant()) : null;
    }
    protected void applyImplicitComponents(DataComponentGetter components) {
        }
    protected <T> boolean applyImplicitComponent(DataComponentType<T> type, T value) {
        return false;
    }
    public SpawnGroupData finalizeSpawn(ServerLevelAccessor level, DifficultyInstance difficulty, EntitySpawnReason spawnReason, SpawnGroupData groupData) {
        return groupData;
    }
    public boolean canBeLeashed() {
        return super.canBeLeashed();
    }
    protected boolean canBeABaby() {
        return false;
    }
}
