// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.monster;
import net.minecraft.core.BlockPos;
import net.minecraft.util.RandomSource;
import net.minecraft.world.DifficultyInstance;
import net.minecraft.world.entity.EntitySpawnReason;
import net.minecraft.world.entity.EntityType;
import net.minecraft.world.entity.SpawnGroupData;
import net.minecraft.world.entity.ai.goal.Goal;
import net.minecraft.world.level.Level;
import net.minecraft.world.level.LevelAccessor;
import net.minecraft.world.level.ServerLevelAccessor;
import net.minecraft.world.level.storage.ValueInput;
import net.minecraft.world.level.storage.ValueOutput;
public abstract class PatrollingMonster extends Monster {
    private BlockPos patrolTarget;
    private boolean patrolLeader;
    protected PatrollingMonster(EntityType<? extends PatrollingMonster> type, Level level) {
        super(type, level);
    }
    protected void registerGoals() {
    }
    protected void addAdditionalSaveData(ValueOutput output) {
    }
    protected void readAdditionalSaveData(ValueInput input) {
    }
    public boolean canBeLeader() {
        return true;
    }
    public SpawnGroupData finalizeSpawn(ServerLevelAccessor level, DifficultyInstance difficulty, EntitySpawnReason spawnReason, SpawnGroupData groupData) {
        return groupData;
    }
    public static boolean checkPatrollingMonsterSpawnRules(EntityType<? extends PatrollingMonster> type, LevelAccessor level, EntitySpawnReason spawnReason, BlockPos pos, RandomSource random) {
        return true;
    }
    public boolean removeWhenFarAway(double distSqr) {
        return super.removeWhenFarAway(distSqr);
    }
    public void setPatrolTarget(BlockPos target) {
        this.patrolTarget = target;
    }
    public BlockPos getPatrolTarget() {
        return patrolTarget;
    }
    public boolean hasPatrolTarget() {
        return patrolTarget != null;
    }
    public void setPatrolLeader(boolean isLeader) {
        this.patrolLeader = isLeader;
        if (live() instanceof org.bukkit.entity.Raider api) { api.setPatrolLeader(isLeader); }
    }
    public boolean isPatrolLeader() {
        return live() instanceof org.bukkit.entity.Raider api ? api.isPatrolLeader() : patrolLeader;
    }
    public boolean canJoinPatrol() {
        return true;
    }
    public void findPatrolTarget() {
        this.patrolTarget = blockPosition().offset(-500 + random.nextInt(1001), 0, -500 + random.nextInt(1001));
    }
    protected boolean isPatrolling() {
        return false;
    }
    protected void setPatrolling(boolean value) {
        }
    public static class LongDistancePatrolGoal<T extends PatrollingMonster> extends Goal {
        public LongDistancePatrolGoal(T mob, double speedModifier, double leaderSpeedModifier) {
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
}
