// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.npc.villager;
import java.util.Map;
import java.util.function.BiPredicate;
import net.minecraft.core.GlobalPos;
import net.minecraft.core.Holder;
import net.minecraft.core.component.DataComponentGetter;
import net.minecraft.core.component.DataComponentType;
import net.minecraft.network.chat.Component;
import net.minecraft.network.syncher.SynchedEntityData;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.sounds.SoundEvent;
import net.minecraft.world.DifficultyInstance;
import net.minecraft.world.InteractionHand;
import net.minecraft.world.InteractionResult;
import net.minecraft.world.damagesource.DamageSource;
import net.minecraft.world.entity.AgeableMob;
import net.minecraft.world.entity.Entity;
import net.minecraft.world.entity.EntityDimensions;
import net.minecraft.world.entity.EntitySpawnReason;
import net.minecraft.world.entity.EntityType;
import net.minecraft.world.entity.LightningBolt;
import net.minecraft.world.entity.LivingEntity;
import net.minecraft.world.entity.Pose;
import net.minecraft.world.entity.ReputationEventHandler;
import net.minecraft.world.entity.SpawnGroupData;
import net.minecraft.world.entity.ai.Brain;
import net.minecraft.world.entity.ai.attributes.AttributeSupplier;
import net.minecraft.world.entity.ai.gossip.GossipContainer;
import net.minecraft.world.entity.ai.memory.MemoryModuleType;
import net.minecraft.world.entity.ai.village.ReputationEventType;
import net.minecraft.world.entity.ai.village.poi.PoiType;
import net.minecraft.world.entity.item.ItemEntity;
import net.minecraft.world.entity.player.Player;
import net.minecraft.world.item.ItemStack;
import net.minecraft.world.item.trading.MerchantOffer;
import net.minecraft.world.item.trading.MerchantOffers;
import net.minecraft.world.level.Level;
import net.minecraft.world.level.ServerLevelAccessor;
import net.minecraft.world.level.storage.ValueInput;
import net.minecraft.world.level.storage.ValueOutput;
public class Villager extends AbstractVillager implements VillagerDataHolder, ReputationEventHandler {
    private GossipContainer gossips = new GossipContainer();

    public static final int BREEDING_FOOD_THRESHOLD = 12;
    public static final float SPEED_MODIFIER = 0.5F;
    public static final Map<MemoryModuleType<GlobalPos>, BiPredicate<Villager, Holder<PoiType>>> POI_MEMORIES = Map.of(
        MemoryModuleType.HOME, (villager, poiType) -> poiType.is(net.minecraft.world.entity.ai.village.poi.PoiTypes.HOME),
        MemoryModuleType.JOB_SITE, (villager, poiType) -> villager.getVillagerData().profession().value().heldJobSite().test(poiType),
        MemoryModuleType.POTENTIAL_JOB_SITE, (villager, poiType) -> VillagerProfession.ALL_ACQUIRABLE_JOBS.test(poiType),
        MemoryModuleType.MEETING_POINT, (villager, poiType) -> poiType.is(net.minecraft.world.entity.ai.village.poi.PoiTypes.MEETING));
    public Villager(EntityType<? extends Villager> entityType, Level level) {
        super(entityType, level);
    }
    public Brain<Villager> getBrain() {
        @SuppressWarnings("unchecked") Brain<Villager> made = (Brain<Villager>) (Brain<?>) super.getBrain(); return made;
    }
    protected Brain<Villager> makeBrain(Brain.Packed packedBrain) {
        @SuppressWarnings("unchecked") Brain<Villager> made = (Brain<Villager>) (Brain<?>) super.makeBrain(packedBrain); return made;
    }
    public void refreshBrain(ServerLevel level) {

    }
    protected void ageBoundaryReached() {
        }
    public static AttributeSupplier.Builder createAttributes() {
        return net.minecraft.world.entity.Mob.createMobAttributes()
            .add(net.minecraft.world.entity.ai.attributes.Attributes.MOVEMENT_SPEED, 0.5);
    }
    protected void customServerAiStep(ServerLevel level) {
    }
    public void tick() {
    }
    public InteractionResult mobInteract(Player player, InteractionHand hand) {
        return net.minecraft.world.InteractionResult.PASS;
    }
    public void setTradingPlayer(Player player) {
        super.setTradingPlayer(player);
    }
    public EntityDimensions getDefaultDimensions(Pose pose) {
        return net.minecraft.world.entity.EntityDimensions.scalable(getBbWidth(), getBbHeight());
    }
    public boolean canRestock() {
        return numberOfRestocksToday() == 0 || numberOfRestocksToday() < 2 && level().getGameTime() > lastRestockGameTime + 2400L;
    }
    public void restock() {
        if (live() instanceof org.bukkit.entity.Villager api) {
            api.restock();
            lastRestockGameTime = level().getGameTime();
        }
    }
    public boolean shouldRestock(ServerLevel level) {
        return canRestock();
    }
    protected void defineSynchedData(SynchedEntityData.Builder entityData) {
    }
    public static VillagerData createDefaultVillagerData() {
        return new VillagerData(
            org.basaltmc.bukkit.facade.VariantHolders.of(net.minecraft.core.registries.Registries.VILLAGER_TYPE, VillagerType.class, org.bukkit.entity.Villager.Type.PLAINS),
            org.basaltmc.bukkit.facade.VariantHolders.of(net.minecraft.core.registries.Registries.VILLAGER_PROFESSION, VillagerProfession.class, org.bukkit.entity.Villager.Profession.NONE), 1);
    }
    protected void addAdditionalSaveData(ValueOutput output) {
    }
    protected void readAdditionalSaveData(ValueInput input) {
    }
    public void postDataManipulated() {

    }
    public boolean removeWhenFarAway(double distSqr) {
        return super.removeWhenFarAway(distSqr);
    }
    protected SoundEvent getAmbientSound() {
        return net.minecraft.sounds.SoundEvents.byId("entity.villager.ambient");
    }
    protected SoundEvent getHurtSound(DamageSource source) {
        return net.minecraft.sounds.SoundEvents.byId("entity.villager.hurt");
    }
    protected SoundEvent getDeathSound() {
        return net.minecraft.sounds.SoundEvents.byId("entity.villager.death");
    }
    public void playWorkSound() {

    }
    public void setVillagerData(VillagerData data) {
        if (live() instanceof org.bukkit.entity.Villager api) {
            org.bukkit.NamespacedKey type = org.bukkit.NamespacedKey.fromString(data.type().unwrapKey().orElseThrow().identifier().toString());
            org.bukkit.NamespacedKey profession = org.bukkit.NamespacedKey.fromString(data.profession().unwrapKey().orElseThrow().identifier().toString());
            api.setVillagerType(org.bukkit.Registry.VILLAGER_TYPE.get(type));
            api.setProfession(org.bukkit.Registry.VILLAGER_PROFESSION.get(profession));
            api.setVillagerLevel(data.level());
        }
    }
    public boolean getVillagerDataFinalized() {
        return villagerDataFinalized;
    }
    public void setVillagerDataFinalized(boolean villagerDataFinalized) {
        this.villagerDataFinalized = villagerDataFinalized;
    }
    public VillagerData getVillagerData() {
        if (live() instanceof org.bukkit.entity.Villager api) {
            return new VillagerData(
                org.basaltmc.bukkit.facade.VariantHolders.of(net.minecraft.core.registries.Registries.VILLAGER_TYPE, VillagerType.class, api.getVillagerType()),
                org.basaltmc.bukkit.facade.VariantHolders.of(net.minecraft.core.registries.Registries.VILLAGER_PROFESSION, VillagerProfession.class, api.getProfession()),
                api.getVillagerLevel());
        }
        return createDefaultVillagerData();
    }
    protected void rewardTradeXp(MerchantOffer offer) {
        }
    public void setLastHurtByMob(LivingEntity hurtBy) {
        super.setLastHurtByMob(hurtBy);
    }
    public void die(DamageSource source) {
        super.die(source);
    }
    public void releasePoi(MemoryModuleType<GlobalPos> memoryType) {

    }
    public boolean canBreed() {
        return live() instanceof org.bukkit.entity.Villager api ? (boolean) api.canBreed() : false;
    }
    public int getPlayerReputation(Player player) {
        if (!(live() instanceof org.bukkit.entity.Villager api)) {
            return 0;
        }
        com.destroystokyo.paper.entity.villager.Reputation reputation = api.getReputation(player.getUUID());
        if (reputation == null) {
            return 0;
        }
        int total = 0;
        for (com.destroystokyo.paper.entity.villager.ReputationType kind : com.destroystokyo.paper.entity.villager.ReputationType.values()) {
            int weight = switch (kind) {
                case MAJOR_NEGATIVE -> -5;
                case MINOR_NEGATIVE -> -1;
                case MINOR_POSITIVE -> 1;
                case MAJOR_POSITIVE -> 5;
                case TRADING -> 1;
            };
            total += reputation.getReputation(kind) * weight;
        }
        return total;
    }
    public void eatAndDigestFood() {

    }
    public void setOffers(MerchantOffers offers) {
        overrideOffers(offers);
    }
    protected Component getTypeName() {
        return net.minecraft.network.chat.Component.translatable(getType().getDescriptionId());
    }
    public void handleEntityEvent(byte id) {
    }
    public SpawnGroupData finalizeSpawn(ServerLevelAccessor level, DifficultyInstance difficulty, EntitySpawnReason spawnReason, SpawnGroupData groupData) {
        return groupData;
    }
    public Villager getBreedOffspring(ServerLevel level, AgeableMob partner) {
        return null;
    }
    public void thunderHit(ServerLevel level, LightningBolt lightningBolt) {
        super.thunderHit(level, lightningBolt);
    }
    protected void pickUpItem(ServerLevel level, ItemEntity entity) {
        }
    public boolean wantsToPickUp(ServerLevel level, ItemStack itemStack) {
        return false;
    }
    public boolean hasExcessFood() {
        return false;
    }
    public boolean wantsMoreFood() {
        return false;
    }
    public boolean hasFarmSeeds() {
        return false;
    }
    protected void updateTrades(ServerLevel level) {
        }
    public void gossip(ServerLevel level, Villager target, long timestamp) {

    }
    public void spawnGolemIfNeeded(ServerLevel level, long timestamp, int villagersNeededToAgree) {

    }
    public boolean wantsToSpawnGolem(long timestamp) {
        return false;
    }
    public void onReputationEventFrom(ReputationEventType type, Entity source) {

    }
    public int getVillagerXp() {
        return live() instanceof org.bukkit.entity.Villager api ? api.getVillagerExperience() : 0;
    }
    public void setVillagerXp(int value) {
        if (live() instanceof org.bukkit.entity.Villager api) {
            api.setVillagerExperience(value);
        }
    }
    public GossipContainer getGossips() {
        return gossips;
    }
    public void setGossips(GossipContainer gossips) {
        this.gossips = gossips;
    }
    public void stopSleeping() {
        if (live() instanceof org.bukkit.entity.Villager api) {
            api.wakeup();
        }
    }
    protected void applyImplicitComponents(DataComponentGetter components) {
        }
    protected <T> boolean applyImplicitComponent(DataComponentType<T> type, T value) {
        return false;
    }
    private boolean villagerDataFinalized;
    private long lastRestockGameTime;

    private int numberOfRestocksToday() {
        return live() instanceof org.bukkit.entity.Villager api ? api.getRestocksToday() : 0;
    }
}
