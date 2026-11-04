// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.animal.fish;
import net.minecraft.world.entity.ai.goal.FollowFlockLeaderGoal;
import java.util.stream.Stream;
import net.minecraft.world.DifficultyInstance;
import net.minecraft.world.entity.EntitySpawnReason;
import net.minecraft.world.entity.EntityType;
import net.minecraft.world.entity.SpawnGroupData;
import net.minecraft.world.level.Level;
import net.minecraft.world.level.ServerLevelAccessor;
public abstract class AbstractSchoolingFish extends AbstractFish {
    public AbstractSchoolingFish(EntityType<? extends AbstractSchoolingFish> type, Level level) {
        super(type, level);
    }
    private AbstractSchoolingFish leader;
    private int schoolSize = 1;
    protected void registerGoals() {
        super.registerGoals();
        goalSelector.addGoal(5, new FollowFlockLeaderGoal(this));
    }
    public int getMaxSpawnClusterSize() {
        return getMaxSchoolSize();
    }
    public int getMaxSchoolSize() {
        return 4;
    }
    protected boolean canRandomSwim() {
        return !isFollower();
    }
    public boolean isFollower() {
        return leader != null && leader.isAlive();
    }
    public AbstractSchoolingFish startFollowing(AbstractSchoolingFish leader) {
        this.leader = leader;
        leader.schoolSize++;
        return this;
    }
    public void stopFollowing() {
        if (leader != null) {
            leader.schoolSize--;
        }
        leader = null;
    }
    public boolean canBeFollowed() {
        return hasFollowers() && schoolSize < getMaxSchoolSize();
    }
    public void tick() {
        super.tick();
        if (hasFollowers() && random.nextInt(200) == 1) {
            java.util.List<? extends AbstractSchoolingFish> nearby = level().getEntitiesOfClass(getClass(), getBoundingBox().inflate(8.0, 8.0, 8.0));
            if (nearby.size() <= 1) {
                schoolSize = 1;
            }
        }
    }
    public boolean hasFollowers() {
        return schoolSize > 1;
    }
    public boolean inRangeOfLeader() {
        return distanceToSqr(leader) <= 121.0;
    }
    public void pathToLeader() {
        if (isFollower()) {
            getNavigation().moveTo(leader, 1.0);
        }
    }
    public void addFollowers(Stream<? extends AbstractSchoolingFish> abstractSchoolingFishStream) {
        abstractSchoolingFishStream.limit(getMaxSchoolSize() - schoolSize).filter(fish -> fish != this).forEach(fish -> fish.startFollowing(this));
    }
    public SpawnGroupData finalizeSpawn(ServerLevelAccessor level, DifficultyInstance difficulty, EntitySpawnReason spawnReason, SpawnGroupData groupData) {
        if (groupData == null) {
            groupData = new SchoolSpawnGroupData(this);
        } else {
            startFollowing(((SchoolSpawnGroupData) groupData).leader);
        }
        return groupData;
    }
    public static class SchoolSpawnGroupData implements SpawnGroupData {
        public final AbstractSchoolingFish leader;
        public SchoolSpawnGroupData(AbstractSchoolingFish leader) {
            this.leader = leader;
        }
    }
}
