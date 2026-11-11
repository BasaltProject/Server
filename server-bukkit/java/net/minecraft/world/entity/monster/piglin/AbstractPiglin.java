// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.monster.piglin;
import net.minecraft.world.attribute.EnvironmentAttributes;
import net.minecraft.network.syncher.EntityDataAccessor;
import net.minecraft.network.syncher.SynchedEntityData;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.world.entity.EntityType;
import net.minecraft.world.entity.LivingEntity;
import net.minecraft.world.entity.monster.Monster;
import net.minecraft.world.level.Level;
import net.minecraft.world.level.storage.ValueInput;
import net.minecraft.world.level.storage.ValueOutput;
public abstract class AbstractPiglin extends Monster {
    private boolean immuneToZombificationFlag;
    protected static final EntityDataAccessor<Boolean> DATA_IMMUNE_TO_ZOMBIFICATION = net.minecraft.network.syncher.EntityDataSerializers.BOOLEAN.createAccessor(16);
    public static final int CONVERSION_TIME = 300;
    protected int timeInOverworld;
    public AbstractPiglin(EntityType<? extends AbstractPiglin> type, Level level) {
        super(type, level);
    }
    protected abstract boolean canHunt();
    public void setImmuneToZombification(boolean isImmuneToZombification) {
        this.immuneToZombificationFlag = isImmuneToZombification;
    }
    protected boolean isImmuneToZombification() {
        return false;
    }
    protected void defineSynchedData(SynchedEntityData.Builder entityData) {
    }
    protected void addAdditionalSaveData(ValueOutput output) {
    }
    protected void readAdditionalSaveData(ValueInput input) {
    }
    protected void customServerAiStep(ServerLevel level) {
    }
    public void setTimeInOverworld(int timeInOverworld) {
        this.timeInOverworld = timeInOverworld;
    }
    public boolean isConverting() {
        return !isImmuneToZombification() && !isNoAi() && level().environmentAttributes().getValue(EnvironmentAttributes.PIGLINS_ZOMBIFY, position());
    }
    protected void finishConversion(ServerLevel level) {
        }
    public boolean isAdult() {
        return !isBaby();
    }
    public abstract PiglinArmPose getArmPose();
    public LivingEntity getTarget() {
        return getTargetFromBrain();
    }
    protected boolean isHoldingMeleeWeapon() {
        return false;
    }
    public void playAmbientSound() {
    }
    protected abstract void playConvertedSound();
}
