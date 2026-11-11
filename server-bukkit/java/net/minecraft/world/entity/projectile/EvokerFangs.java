// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.projectile;
import net.minecraft.world.entity.EntityReference;
import net.minecraft.network.syncher.SynchedEntityData;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.world.damagesource.DamageSource;
import net.minecraft.world.entity.Entity;
import net.minecraft.world.entity.EntityType;
import net.minecraft.world.entity.LivingEntity;
import net.minecraft.world.entity.TraceableEntity;
import net.minecraft.world.level.Level;
import net.minecraft.world.level.storage.ValueInput;
import net.minecraft.world.level.storage.ValueOutput;
public class EvokerFangs extends Entity implements TraceableEntity {
    private boolean clientSideAttackStarted;
    private int lifeTicks = 22;
    private EntityReference<LivingEntity> owner;
    public static final int ATTACK_DURATION = 20;
    public static final int LIFE_OFFSET = 2;
    public static final int ATTACK_TRIGGER_TICKS = 14;
    public EvokerFangs(EntityType<? extends EvokerFangs> type, Level level) {
        super(type, level);
    }
    public EvokerFangs(Level level, double x, double y, double z, float rotaionRadians, int warmupDelayTicks, LivingEntity owner) {
        super((net.minecraft.world.entity.EntityType) null, (net.minecraft.world.level.Level) null);
    }
    protected void defineSynchedData(SynchedEntityData.Builder entityData) {
    }
    public void setOwner(LivingEntity owner) {
        this.owner = EntityReference.of(owner);
    }
    public LivingEntity getOwner() {
        return EntityReference.getLivingEntity(owner, level());
    }
    protected void readAdditionalSaveData(ValueInput input) {
    }
    protected void addAdditionalSaveData(ValueOutput output) {
    }
    public void tick() {
    }
    public void handleEntityEvent(byte id) {
    }
    public float getAnimationProgress(float a) {
        if (!clientSideAttackStarted) {
            return 0.0F;
        }
        int remaining = lifeTicks - 2;
        return remaining <= 0 ? 1.0F : 1.0F - (remaining - a) / 20.0F;
    }
    public boolean hurtServer(ServerLevel level, DamageSource source, float damage) {
        return super.hurtServer(level, source, damage);
    }
}
