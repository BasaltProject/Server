// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.animal.nautilus;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.sounds.SoundEvent;
import net.minecraft.world.damagesource.DamageSource;
import net.minecraft.world.entity.AgeableMob;
import net.minecraft.world.entity.EntityDimensions;
import net.minecraft.world.entity.EntityType;
import net.minecraft.world.entity.Pose;
import net.minecraft.world.entity.ai.Brain;
import net.minecraft.world.level.Level;
public class Nautilus extends AbstractNautilus {
    public Nautilus(EntityType<? extends Nautilus> type, Level level) {
        super(type, level);
    }
    protected Brain<Nautilus> makeBrain(Brain.Packed packedBrain) {
        @SuppressWarnings("unchecked") Brain<Nautilus> made = (Brain<Nautilus>) (Brain<?>) super.makeBrain(packedBrain); return made;
    }
    public Brain<Nautilus> getBrain() {
        @SuppressWarnings("unchecked") Brain<Nautilus> made = (Brain<Nautilus>) (Brain<?>) super.getBrain(); return made;
    }
    public Nautilus getBreedOffspring(ServerLevel level, AgeableMob partner) {
        return null;
    }
    public EntityDimensions getDefaultDimensions(Pose pose) {
        return net.minecraft.world.entity.EntityDimensions.scalable(getBbWidth(), getBbHeight());
    }
    protected void customServerAiStep(ServerLevel level) {
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
    public int getMaxAirSupply() {
        return 300;
    }
    protected void handleAirSupply(ServerLevel level, int preTickAirSupply) {
        }
    public void baseTick() {
    }
    public boolean canBeLeashed() {
        return super.canBeLeashed();
    }
}
