// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.animal.equine;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.sounds.SoundEvent;
import net.minecraft.world.damagesource.DamageSource;
import net.minecraft.world.entity.AgeableMob;
import net.minecraft.world.entity.EntityType;
import net.minecraft.world.entity.animal.Animal;
import net.minecraft.world.level.Level;
public class Donkey extends AbstractChestedHorse {
    public Donkey(EntityType<? extends Donkey> type, Level level) {
        super(type, level);
    }
    protected SoundEvent getAmbientSound() {
        return net.minecraft.sounds.SoundEvents.byId("entity.donkey.ambient");
    }
    protected SoundEvent getAngrySound() {
        return null;
    }
    protected SoundEvent getDeathSound() {
        return net.minecraft.sounds.SoundEvents.byId("entity.donkey.death");
    }
    protected SoundEvent getEatingSound() {
        return null;
    }
    protected SoundEvent getHurtSound(DamageSource source) {
        return net.minecraft.sounds.SoundEvents.byId("entity.donkey.hurt");
    }
    public boolean canMate(Animal partner) {
        return partner != this && partner.getClass() == getClass() && live() instanceof org.bukkit.entity.Animals selfApi && selfApi.isLoveMode() && partner.live() instanceof org.bukkit.entity.Animals otherApi && otherApi.isLoveMode();
    }
    protected void playJumpSound() {
        }
    public AgeableMob getBreedOffspring(ServerLevel level, AgeableMob partner) {
        return null;
    }
}
