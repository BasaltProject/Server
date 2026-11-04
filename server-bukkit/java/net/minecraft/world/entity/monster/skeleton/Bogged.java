// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.monster.skeleton;
import net.minecraft.network.syncher.SynchedEntityData;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.sounds.SoundEvent;
import net.minecraft.sounds.SoundSource;
import net.minecraft.world.InteractionHand;
import net.minecraft.world.InteractionResult;
import net.minecraft.world.damagesource.DamageSource;
import net.minecraft.world.entity.EntityType;
import net.minecraft.world.entity.Shearable;
import net.minecraft.world.entity.ai.attributes.AttributeSupplier;
import net.minecraft.world.entity.player.Player;
import net.minecraft.world.entity.projectile.arrow.AbstractArrow;
import net.minecraft.world.item.ItemStack;
import net.minecraft.world.level.Level;
import net.minecraft.world.level.storage.ValueInput;
import net.minecraft.world.level.storage.ValueOutput;
public class Bogged extends AbstractSkeleton implements Shearable {
    public static AttributeSupplier.Builder createAttributes() {
        return net.minecraft.world.entity.Mob.createMobAttributes();
    }
    public Bogged(EntityType<? extends Bogged> type, Level level) {
        super(type, level);
    }
    protected void defineSynchedData(SynchedEntityData.Builder entityData) {
    }
    protected void addAdditionalSaveData(ValueOutput output) {
    }
    protected void readAdditionalSaveData(ValueInput input) {
    }
    public boolean isSheared() {
        return live() instanceof org.bukkit.entity.Bogged api ? (boolean) api.isSheared() : false;
    }
    public void setSheared(boolean sheared) {
        if (live() instanceof org.bukkit.entity.Bogged api) { api.setSheared(sheared); }
    }
    protected InteractionResult mobInteract(Player player, InteractionHand hand) {
        return net.minecraft.world.InteractionResult.PASS;
    }
    protected SoundEvent getAmbientSound() {
        return net.minecraft.sounds.SoundEvents.byId("entity.bogged.ambient");
    }
    protected SoundEvent getHurtSound(DamageSource source) {
        return net.minecraft.sounds.SoundEvents.byId("entity.bogged.hurt");
    }
    protected SoundEvent getDeathSound() {
        return net.minecraft.sounds.SoundEvents.byId("entity.bogged.death");
    }
    protected SoundEvent getStepSound() {
        return net.minecraft.sounds.SoundEvents.BOGGED_STEP;
    }
    protected AbstractArrow getArrow(ItemStack projectile, float power, ItemStack firingWeapon) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.entity.monster.skeleton.Bogged#getArrow");
    }
    protected int getHardAttackInterval() {
        return 0;
    }
    protected int getAttackInterval() {
        return 0;
    }
    public void shear(ServerLevel level, SoundSource soundSource, ItemStack tool) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.entity.monster.skeleton.Bogged#shear");
    }
    public boolean readyForShearing() {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.entity.monster.skeleton.Bogged#readyForShearing");
    }
}
