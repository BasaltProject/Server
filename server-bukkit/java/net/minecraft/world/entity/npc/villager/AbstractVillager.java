// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.npc.villager;
import net.minecraft.util.Mth;
import java.util.Optional;
import net.minecraft.world.item.trading.ItemCost;
import net.minecraft.core.HolderSet;
import net.minecraft.core.particles.ParticleOptions;
import net.minecraft.network.syncher.SynchedEntityData;
import net.minecraft.resources.ResourceKey;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.sounds.SoundEvent;
import net.minecraft.world.DifficultyInstance;
import net.minecraft.world.SimpleContainer;
import net.minecraft.world.damagesource.DamageSource;
import net.minecraft.world.entity.AgeableMob;
import net.minecraft.world.entity.Entity;
import net.minecraft.world.entity.EntitySpawnReason;
import net.minecraft.world.entity.EntityType;
import net.minecraft.world.entity.SlotAccess;
import net.minecraft.world.entity.SpawnGroupData;
import net.minecraft.world.entity.npc.InventoryCarrier;
import net.minecraft.world.entity.npc.Npc;
import net.minecraft.world.entity.player.Player;
import net.minecraft.world.item.ItemStack;
import net.minecraft.world.item.trading.Merchant;
import net.minecraft.world.item.trading.MerchantOffer;
import net.minecraft.world.item.trading.MerchantOffers;
import net.minecraft.world.item.trading.TradeSet;
import net.minecraft.world.item.trading.VillagerTrade;
import net.minecraft.world.level.Level;
import net.minecraft.world.level.ServerLevelAccessor;
import net.minecraft.world.level.portal.TeleportTransition;
import net.minecraft.world.level.storage.ValueInput;
import net.minecraft.world.level.storage.ValueOutput;
import net.minecraft.world.level.storage.loot.LootContext;
import net.minecraft.world.phys.Vec3;
public abstract class AbstractVillager extends AgeableMob implements Npc, Merchant, InventoryCarrier {
    protected MerchantOffers offers;
    public AbstractVillager(EntityType<? extends AbstractVillager> type, Level level) {
        super(type, level);
    }
    private MerchantOffers readOffers() {
        MerchantOffers result = new MerchantOffers();
        if (!(live() instanceof org.bukkit.entity.AbstractVillager api)) {
            return result;
        }
        for (org.bukkit.inventory.MerchantRecipe recipe : api.getRecipes()) {
            java.util.List<org.bukkit.inventory.ItemStack> ingredients = recipe.getIngredients();
            if (ingredients.isEmpty()) {
                continue;
            }
            ItemStack first = org.bukkit.craftbukkit.inventory.CraftItemStack.asNMSCopy(ingredients.get(0));
            Optional<ItemCost> second = Optional.empty();
            if (ingredients.size() > 1 && !ingredients.get(1).getType().isAir()) {
                ItemStack other = org.bukkit.craftbukkit.inventory.CraftItemStack.asNMSCopy(ingredients.get(1));
                second = Optional.of(new ItemCost(other.getItem(), other.getCount()));
            }
            MerchantOffer offer = new MerchantOffer(new ItemCost(first.getItem(), first.getCount()), second,
                org.bukkit.craftbukkit.inventory.CraftItemStack.asNMSCopy(recipe.getResult()), recipe.getUses(), recipe.getMaxUses(),
                recipe.getVillagerExperience(), recipe.getPriceMultiplier(), recipe.getDemand());
            offer.setSpecialPriceDiff(recipe.getSpecialPrice());
            result.add(offer);
        }
        return result;
    }
    private void writeOffers() {
        if (!(live() instanceof org.bukkit.entity.AbstractVillager api) || offers == null) {
            return;
        }
        java.util.List<org.bukkit.inventory.MerchantRecipe> recipes = new java.util.ArrayList<>();
        for (MerchantOffer offer : offers) {
            org.bukkit.inventory.MerchantRecipe recipe = new org.bukkit.inventory.MerchantRecipe(
                org.bukkit.craftbukkit.inventory.CraftItemStack.asCraftMirror(offer.getResult()), offer.getUses(), offer.getMaxUses(),
                offer.shouldRewardExp(), offer.getXp(), offer.getPriceMultiplier(), offer.getDemand(), offer.getSpecialPriceDiff());
            recipe.addIngredient(org.bukkit.craftbukkit.inventory.CraftItemStack.asCraftMirror(offer.getBaseCostA()));
            if (!offer.getCostB().isEmpty()) {
                recipe.addIngredient(org.bukkit.craftbukkit.inventory.CraftItemStack.asCraftMirror(offer.getCostB()));
            }
            recipes.add(recipe);
        }
        api.setRecipes(recipes);
    }
    public SpawnGroupData finalizeSpawn(ServerLevelAccessor level, DifficultyInstance difficulty, EntitySpawnReason spawnReason, SpawnGroupData groupData) {
        return groupData;
    }
    public int getUnhappyCounter() {
        return unhappyCounter;
    }
    public void setUnhappyCounter(int value) {
        unhappyCounter = value;
    }
    public int getVillagerXp() {
        return live() instanceof org.bukkit.entity.Villager api ? api.getVillagerExperience() : 0;
    }
    protected void defineSynchedData(SynchedEntityData.Builder entityData) {
    }
    public void setTradingPlayer(Player player) {
        tradingPlayer = player;
    }
    public Player getTradingPlayer() {
        if (tradingPlayer != null) {
            return tradingPlayer;
        }
        if (live() instanceof org.bukkit.entity.AbstractVillager api && api.getTrader() != null) {
            return (Player) Entity.fromBukkit(api.getTrader());
        }
        return null;
    }
    public boolean isTrading() {
        return tradingPlayer != null || live() instanceof org.bukkit.entity.AbstractVillager api && api.isTrading();
    }
    public MerchantOffers getOffers() {
        if (offers == null) {
            offers = readOffers();
        }
        return offers;
    }
    public void overrideOffers(MerchantOffers offers) {
        this.offers = offers;
        writeOffers();
    }
    public void overrideXp(int xp) {
        if (live() instanceof org.bukkit.entity.Villager api) { api.setVillagerExperience(xp); }
    }
    public void notifyTrade(MerchantOffer offer) {
        offer.increaseUses();
        ambientSoundTime = -getAmbientSoundInterval();
        rewardTradeXp(offer);
        writeOffers();
    }
    protected abstract void rewardTradeXp(MerchantOffer offer);
    public boolean showProgressBar() {
        return true;
    }
    public void notifyTradeUpdated(ItemStack itemStack) {
        if (!itemStack.isEmpty() && ambientSoundTime > -getAmbientSoundInterval() + 20) {
            ambientSoundTime = -getAmbientSoundInterval();
        }
    }
    public SoundEvent getNotifyTradeSound() {
        return null;
    }
    protected SoundEvent getTradeUpdatedSound(boolean validTrade) {
        return null;
    }
    public void playCelebrateSound() {
        playSound(getNotifyTradeSound(), getSoundVolume(), getVoicePitch());
    }
    protected void addAdditionalSaveData(ValueOutput output) {
    }
    protected void readAdditionalSaveData(ValueInput input) {
    }
    public Entity teleport(TeleportTransition transition) {
        stopTrading();
        return super.teleport(transition);
    }
    protected void stopTrading() {
        setTradingPlayer(null);
    }
    public void die(DamageSource source) {
        super.die(source);
        stopTrading();
    }
    protected void addParticlesAroundSelf(ParticleOptions particle) {
        }
    public boolean canBeLeashed() {
        return super.canBeLeashed();
    }
    public SimpleContainer getInventory() {
        SimpleContainer container = new SimpleContainer(8);
        if (live() instanceof org.bukkit.entity.AbstractVillager api) {
            org.bukkit.inventory.ItemStack[] contents = api.getInventory().getContents();
            for (int i = 0; i < contents.length && i < 8; i++) {
                if (contents[i] != null) {
                    container.setItem(i, org.bukkit.craftbukkit.inventory.CraftItemStack.asNMSCopy(contents[i]));
                }
            }
        }
        return container;
    }
    public SlotAccess getSlot(int slot) {
        return super.getSlot(slot);
    }
    protected abstract void updateTrades(ServerLevel level);
    protected void addOffersFromTradeSet(ServerLevel level, MerchantOffers offers, ResourceKey<TradeSet> resourceKey) {
        }
    static void addOffersFromItemListings(LootContext lootContext, MerchantOffers merchantOffers, HolderSet<VillagerTrade> potentialOffers, int numberOfOffers) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.entity.npc.villager.AbstractVillager#addOffersFromItemListings");
    }
    public Vec3 getRopeHoldPosition(float partialTickTime) {
        float yaw = Mth.lerp(partialTickTime, yBodyRotO, yBodyRot) * (float) (Math.PI / 180.0);
        return getPosition(partialTickTime).add(new Vec3(0.0, getBoundingBox().getYsize() - 1.0, 0.2).yRot(-yaw));
    }
    public boolean isClientSide() {
        return false;
    }
    public boolean stillValid(Player player) {
        return getTradingPlayer() == player;
    }
    private Player tradingPlayer;
    private int unhappyCounter;
}
