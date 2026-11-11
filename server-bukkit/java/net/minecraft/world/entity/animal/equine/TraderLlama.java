// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.animal.equine;
import net.minecraft.world.entity.EntityTypes;
import net.minecraft.world.DifficultyInstance;
import net.minecraft.world.entity.EntitySpawnReason;
import net.minecraft.world.entity.EntityType;
import net.minecraft.world.entity.SpawnGroupData;
import net.minecraft.world.entity.ai.goal.target.TargetGoal;
import net.minecraft.world.entity.player.Player;
import net.minecraft.world.level.Level;
import net.minecraft.world.level.ServerLevelAccessor;
import net.minecraft.world.level.storage.ValueInput;
import net.minecraft.world.level.storage.ValueOutput;
public class TraderLlama extends Llama {
    private int despawnDelay;
    public TraderLlama(EntityType<? extends TraderLlama> type, Level level) {
        super(type, level);
    }
    public boolean isTraderLlama() {
        return true;
    }
    protected Llama makeNewLlama() {
        TraderLlama baby = (TraderLlama) EntityTypes.TRADER_LLAMA.create(level(), EntitySpawnReason.BREEDING);
        if (baby != null) {
            baby.setPersistenceRequired();
        }
        return baby;
    }
    protected void addAdditionalSaveData(ValueOutput output) {
    }
    protected void readAdditionalSaveData(ValueInput input) {
    }
    protected void registerGoals() {
    }
    public void setDespawnDelay(int despawnDelay) {
        this.despawnDelay = despawnDelay;


    }
    protected void doPlayerRide(Player player) {
        }
    public void aiStep() {
    }
    public SpawnGroupData finalizeSpawn(ServerLevelAccessor level, DifficultyInstance difficulty, EntitySpawnReason spawnReason, SpawnGroupData groupData) {
        return groupData;
    }
    protected static class TraderLlamaDefendWanderingTraderGoal extends TargetGoal {
        public TraderLlamaDefendWanderingTraderGoal(Llama tameAnimal) {
            super((net.minecraft.world.entity.Mob) null, false);
        }
        public boolean canUse() {
            return false;
        }
        public void start() {
            
        }
    }
}
