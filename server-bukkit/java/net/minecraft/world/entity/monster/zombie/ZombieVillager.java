// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.monster.zombie;
import net.minecraft.core.component.DataComponentGetter;
import net.minecraft.core.component.DataComponentType;
import net.minecraft.network.syncher.SynchedEntityData;
import net.minecraft.sounds.SoundEvent;
import net.minecraft.world.DifficultyInstance;
import net.minecraft.world.InteractionHand;
import net.minecraft.world.InteractionResult;
import net.minecraft.world.damagesource.DamageSource;
import net.minecraft.world.entity.EntityDimensions;
import net.minecraft.world.entity.EntitySpawnReason;
import net.minecraft.world.entity.EntityType;
import net.minecraft.world.entity.Pose;
import net.minecraft.world.entity.SpawnGroupData;
import net.minecraft.world.entity.ai.gossip.GossipContainer;
import net.minecraft.world.entity.npc.villager.VillagerData;
import net.minecraft.world.entity.npc.villager.VillagerDataHolder;
import net.minecraft.world.entity.player.Player;
import net.minecraft.world.item.trading.MerchantOffers;
import net.minecraft.world.level.Level;
import net.minecraft.world.level.ServerLevelAccessor;
import net.minecraft.world.level.storage.ValueInput;
import net.minecraft.world.level.storage.ValueOutput;
public class ZombieVillager extends Zombie implements VillagerDataHolder {
    public ZombieVillager(EntityType<? extends ZombieVillager> type, Level level) {
        super(type, level);
    }
    protected void defineSynchedData(SynchedEntityData.Builder entityData) {
    }
    protected void addAdditionalSaveData(ValueOutput output) {
    }
    protected void readAdditionalSaveData(ValueInput input) {
    }
    public SpawnGroupData finalizeSpawn(ServerLevelAccessor level, DifficultyInstance difficulty, EntitySpawnReason spawnReason, SpawnGroupData groupData) {
        return groupData;
    }
    public void tick() {
    }
    public InteractionResult mobInteract(Player player, InteractionHand hand) {
        return net.minecraft.world.InteractionResult.PASS;
    }
    protected boolean convertsInWater() {
        return false;
    }
    public boolean removeWhenFarAway(double distSqr) {
        return super.removeWhenFarAway(distSqr);
    }
    public boolean isConverting() {
        return live() instanceof org.bukkit.entity.ZombieVillager api ? (boolean) api.isConverting() : false;
    }
    public EntityDimensions getDefaultDimensions(Pose pose) {
        return net.minecraft.world.entity.EntityDimensions.scalable(getBbWidth(), getBbHeight());
    }
    public void handleEntityEvent(byte id) {
    }
    public void setVillagerConversionTime(int conversionTime) {
        if (live() instanceof org.bukkit.entity.ZombieVillager api) {
            api.setConversionTime(conversionTime);
        }
    }
    public SoundEvent getAmbientSound() {
        return net.minecraft.sounds.SoundEvents.ZOMBIE_VILLAGER_AMBIENT;
    }
    public SoundEvent getHurtSound(DamageSource source) {
        return net.minecraft.sounds.SoundEvents.ZOMBIE_VILLAGER_HURT;
    }
    public SoundEvent getDeathSound() {
        return net.minecraft.sounds.SoundEvents.ZOMBIE_VILLAGER_DEATH;
    }
    public SoundEvent getStepSound() {
        return net.minecraft.sounds.SoundEvents.ZOMBIE_VILLAGER_STEP;
    }
    public void setTradeOffers(MerchantOffers tradeOffers) {
        this.tradeOffers = tradeOffers;
    }
    public void setGossips(GossipContainer gossips) {
        this.gossips = gossips;
    }
    public void setVillagerData(VillagerData data) {
        if (live() instanceof org.bukkit.entity.ZombieVillager api) {
            api.setVillagerType(org.bukkit.Registry.VILLAGER_TYPE.get(org.bukkit.NamespacedKey.fromString(data.type().unwrapKey().orElseThrow().identifier().toString())));
            api.setVillagerProfession(org.bukkit.Registry.VILLAGER_PROFESSION.get(org.bukkit.NamespacedKey.fromString(data.profession().unwrapKey().orElseThrow().identifier().toString())));
        }
        this.data = data;
    }
    public boolean getVillagerDataFinalized() {
        return villagerDataFinalized;
    }
    public void setVillagerDataFinalized(boolean villagerDataFinalized) {
        this.villagerDataFinalized = villagerDataFinalized;
    }
    public VillagerData getVillagerData() {
        if (live() instanceof org.bukkit.entity.ZombieVillager api) {
            return new VillagerData(
                org.basaltmc.bukkit.facade.VariantHolders.of(net.minecraft.core.registries.Registries.VILLAGER_TYPE, net.minecraft.world.entity.npc.villager.VillagerType.class, api.getVillagerType()),
                org.basaltmc.bukkit.facade.VariantHolders.of(net.minecraft.core.registries.Registries.VILLAGER_PROFESSION, net.minecraft.world.entity.npc.villager.VillagerProfession.class, api.getVillagerProfession()),
                data == null ? 1 : data.level());
        }
        return data == null ? net.minecraft.world.entity.npc.villager.Villager.createDefaultVillagerData() : data;
    }
    public int getVillagerXp() {
        return villagerXp;
    }
    public void setVillagerXp(int villagerXp) {
        this.villagerXp = villagerXp;
    }
    protected void applyImplicitComponents(DataComponentGetter components) {
        }
    protected <T> boolean applyImplicitComponent(DataComponentType<T> type, T value) {
        return false;
    }
    private MerchantOffers tradeOffers;
    private GossipContainer gossips = new GossipContainer();
    private VillagerData data;
    private boolean villagerDataFinalized;
    private int villagerXp;

}
