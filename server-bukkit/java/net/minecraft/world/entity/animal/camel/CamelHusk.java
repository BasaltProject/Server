// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.animal.camel;
import net.minecraft.world.entity.Mob;
import net.minecraft.sounds.SoundEvents;
import net.minecraft.core.BlockPos;
import net.minecraft.core.Holder;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.sounds.SoundEvent;
import net.minecraft.world.InteractionHand;
import net.minecraft.world.InteractionResult;
import net.minecraft.world.damagesource.DamageSource;
import net.minecraft.world.entity.AgeableMob;
import net.minecraft.world.entity.EntityType;
import net.minecraft.world.entity.animal.Animal;
import net.minecraft.world.entity.player.Player;
import net.minecraft.world.item.ItemStack;
import net.minecraft.world.level.Level;
import net.minecraft.world.level.block.state.BlockState;
import net.minecraft.world.phys.Vec3;
public class CamelHusk extends Camel {
    public CamelHusk(EntityType<? extends Camel> type, Level level) {
        super(type, level);
    }
    public boolean removeWhenFarAway(double distSqr) {
        return super.removeWhenFarAway(distSqr);
    }
    public boolean isMobControlled() {
        return getFirstPassenger() instanceof Mob;
    }
    public InteractionResult interact(Player player, InteractionHand hand, Vec3 location) {
        return net.minecraft.world.InteractionResult.PASS;
    }
    public boolean canBeLeashed() {
        return super.canBeLeashed();
    }
    public boolean isFood(ItemStack itemStack) {
        return live() instanceof org.bukkit.entity.Animals animals && animals.isBreedItem(org.bukkit.craftbukkit.inventory.CraftItemStack.asCraftMirror(itemStack));
    }
    protected SoundEvent getAmbientSound() {
        return null;
    }
    public boolean canMate(Animal partner) {
        return partner != this && partner.getClass() == getClass() && live() instanceof org.bukkit.entity.Animals selfApi && selfApi.isLoveMode() && partner.live() instanceof org.bukkit.entity.Animals otherApi && otherApi.isLoveMode();
    }
    public Camel getBreedOffspring(ServerLevel level, AgeableMob partner) {
        return null;
    }
    public boolean canFallInLove() {
        return false;
    }
    public boolean canBeABaby() {
        return false;
    }
    protected SoundEvent getDeathSound() {
        return null;
    }
    protected SoundEvent getHurtSound(DamageSource source) {
        return null;
    }
    protected void playStepSound(BlockPos pos, BlockState blockState) {
    }
    protected SoundEvent getDashingSound() {
        return null;
    }
    protected SoundEvent getDashReadySound() {
        return null;
    }
    protected SoundEvent getEatingSound() {
        return null;
    }
    protected SoundEvent getStandUpSound() {
        return null;
    }
    protected SoundEvent getSitDownSound() {
        return null;
    }
    protected Holder.Reference<SoundEvent> getSaddleSound() {
        return SoundEvents.CAMEL_HUSK_SADDLE;
    }
    public float chargeSpeedModifier() {
        return 4.0F;


    }
}
