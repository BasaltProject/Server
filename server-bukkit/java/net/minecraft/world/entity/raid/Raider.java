// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.raid;
import net.minecraft.world.entity.EquipmentSlot;
import net.minecraft.world.item.Items;
import net.minecraft.network.syncher.EntityDataAccessor;
import net.minecraft.network.syncher.SynchedEntityData;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.sounds.SoundEvent;
import net.minecraft.world.DifficultyInstance;
import net.minecraft.world.damagesource.DamageSource;
import net.minecraft.world.entity.EntitySpawnReason;
import net.minecraft.world.entity.EntityType;
import net.minecraft.world.entity.SpawnGroupData;
import net.minecraft.world.entity.ai.goal.Goal;
import net.minecraft.world.entity.ai.targeting.TargetingConditions;
import net.minecraft.world.entity.item.ItemEntity;
import net.minecraft.world.entity.monster.PatrollingMonster;
import net.minecraft.world.entity.monster.illager.AbstractIllager;
import net.minecraft.world.level.Level;
import net.minecraft.world.level.ServerLevelAccessor;
import net.minecraft.world.level.storage.ValueInput;
import net.minecraft.world.level.storage.ValueOutput;
public abstract class Raider extends PatrollingMonster {
    protected static final EntityDataAccessor<Boolean> IS_CELEBRATING = null;
    protected Raid raid;
    protected Raider(EntityType<? extends Raider> type, Level level) {
        super(type, level);
    }
    protected void registerGoals() {
    }
    protected void defineSynchedData(SynchedEntityData.Builder entityData) {
    }
    public abstract void applyRaidBuffs(ServerLevel level, int wave, boolean isCaptain);
    public boolean canJoinRaid() {
        return live() instanceof org.bukkit.entity.Raider api && api.isCanJoinRaid();
    }
    public void setCanJoinRaid(boolean canJoinRaid) {
        if (live() instanceof org.bukkit.entity.Raider api) { api.setCanJoinRaid(canJoinRaid); }
    }
    public void aiStep() {
    }
    protected void updateNoActionTime() {
        }
    public void die(DamageSource source) {
        super.die(source);
    }
    public boolean canJoinPatrol() {
        return !hasActiveRaid();
    }
    public void setCurrentRaid(Raid raid) {
        this.raid = raid;
    }
    public Raid getCurrentRaid() {
        return raid;
    }
    public boolean isCaptain() {
        return getItemBySlot(EquipmentSlot.HEAD).getItem().toString().endsWith("banner") && live() instanceof org.bukkit.entity.Raider api && api.isPatrolLeader();
    }
    public boolean hasRaid() {
        return raid != null || live() instanceof org.bukkit.entity.Raider api && api.getRaid() != null;
    }
    public boolean hasActiveRaid() {
        return hasRaid();
    }
    public void setWave(int wave) {
        if (live() instanceof org.bukkit.entity.Raider api) { api.setWave(wave); }
    }
    public int getWave() {
        return live() instanceof org.bukkit.entity.Raider api ? (int) api.getWave() : 0;
    }
    public boolean isCelebrating() {
        return live() instanceof org.bukkit.entity.Raider api ? (boolean) api.isCelebrating() : false;
    }
    public void setCelebrating(boolean celebrating) {
        if (live() instanceof org.bukkit.entity.Raider api) { api.setCelebrating(celebrating); }
    }
    protected void addAdditionalSaveData(ValueOutput output) {
    }
    protected void readAdditionalSaveData(ValueInput input) {
    }
    protected void pickUpItem(ServerLevel level, ItemEntity entity) {
        }
    public boolean removeWhenFarAway(double distSqr) {
        return super.removeWhenFarAway(distSqr);
    }
    public boolean requiresCustomPersistence() {
        return hasRaid();
    }
    public int getTicksOutsideRaid() {
        return live() instanceof org.bukkit.entity.Raider api ? (int) api.getTicksOutsideRaid() : 0;
    }
    public void setTicksOutsideRaid(int ticksOutsideRaid) {
        if (live() instanceof org.bukkit.entity.Raider api) { api.setTicksOutsideRaid(ticksOutsideRaid); }
    }
    public boolean hurtServer(ServerLevel level, DamageSource source, float damage) {
        return super.hurtServer(level, source, damage);
    }
    public SpawnGroupData finalizeSpawn(ServerLevelAccessor level, DifficultyInstance difficulty, EntitySpawnReason spawnReason, SpawnGroupData groupData) {
        return groupData;
    }
    public abstract SoundEvent getCelebrateSound();
    public class ObtainRaidLeaderBannerGoal<T extends Raider> extends Goal {
        public ObtainRaidLeaderBannerGoal(T mob) {
            super();
        }
        public boolean canUse() {
            return false;
        }
        public boolean canContinueToUse() {
            return false;
        }
        public void start() {
            
        }
        public void stop() {
            
        }
        public void tick() {
        }
    }
    public class RaiderCelebration extends Goal {
        public RaiderCelebration(Raider mob) {
            super();
        }
        public boolean canUse() {
            return false;
        }
        public void start() {
            
        }
        public void stop() {
            
        }
        public void tick() {
        }
    }
    protected static class HoldGroundAttackGoal extends Goal {
        public final TargetingConditions shoutTargeting = null;
        public HoldGroundAttackGoal(AbstractIllager mob, float hostileRadius) {
            super();
        }
        public boolean canUse() {
            return false;
        }
        public void start() {
            
        }
        public void stop() {
            
        }
        public boolean requiresUpdateEveryTick() {
            return false;
        }
        public void tick() {
        }
    }
    static class RaiderMoveThroughVillageGoal extends Goal {
        public RaiderMoveThroughVillageGoal(Raider mob, double speedModifier, int distanceToPoi) {
            super();
        }
        public boolean canUse() {
            return false;
        }
        public boolean canContinueToUse() {
            return false;
        }
        public void stop() {
            
        }
        public void start() {
            
        }
        public void tick() {
        }
    }
}
