// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.npc.wanderingtrader;
import net.minecraft.core.BlockPos;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.sounds.SoundEvent;
import net.minecraft.world.InteractionHand;
import net.minecraft.world.InteractionResult;
import net.minecraft.world.damagesource.DamageSource;
import net.minecraft.world.entity.AgeableMob;
import net.minecraft.world.entity.EntityType;
import net.minecraft.world.entity.ai.goal.Goal;
import net.minecraft.world.entity.npc.villager.AbstractVillager;
import net.minecraft.world.entity.player.Player;
import net.minecraft.world.item.ItemStack;
import net.minecraft.world.item.component.Consumable;
import net.minecraft.world.item.trading.MerchantOffer;
import net.minecraft.world.level.Level;
import net.minecraft.world.level.storage.ValueInput;
import net.minecraft.world.level.storage.ValueOutput;
public class WanderingTrader extends AbstractVillager implements Consumable.OverrideConsumeSound {
    private BlockPos wanderTarget;
    public WanderingTrader(EntityType<? extends WanderingTrader> type, Level level) {
        super(type, level);
    }
    protected void registerGoals() {
    }
    public AgeableMob getBreedOffspring(ServerLevel level, AgeableMob partner) {
        return null;
    }
    public boolean showProgressBar() {
        return false;
    }
    public InteractionResult mobInteract(Player player, InteractionHand hand) {
        return net.minecraft.world.InteractionResult.PASS;
    }
    protected void updateTrades(ServerLevel level) {
        }
    protected void addAdditionalSaveData(ValueOutput output) {
    }
    protected void readAdditionalSaveData(ValueInput input) {
    }
    public boolean removeWhenFarAway(double distSqr) {
        return super.removeWhenFarAway(distSqr);
    }
    protected void rewardTradeXp(MerchantOffer offer) {
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
    public SoundEvent getConsumeSound(ItemStack itemStack) {
        return null;
    }
    protected SoundEvent getTradeUpdatedSound(boolean validTrade) {
        return null;
    }
    public SoundEvent getNotifyTradeSound() {
        return null;
    }
    public void setDespawnDelay(int despawnDelay) {
        if (live() instanceof org.bukkit.entity.WanderingTrader api) { api.setDespawnDelay(despawnDelay); }
    }
    public int getDespawnDelay() {
        return live() instanceof org.bukkit.entity.WanderingTrader api ? (int) api.getDespawnDelay() : 0;
    }
    public void aiStep() {
    }
    public void setWanderTarget(BlockPos pos) {
        this.wanderTarget = pos;
    }
    class WanderToPositionGoal extends Goal {
        public WanderToPositionGoal(WanderingTrader trader, double stopDistance, double speedModifier) {
            super();
        }
        public void stop() {
            
        }
        public boolean canUse() {
            return false;
        }
        public void tick() {
        }
    }
}
