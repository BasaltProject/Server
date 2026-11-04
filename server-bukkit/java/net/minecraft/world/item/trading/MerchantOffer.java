// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.item.trading;
import com.mojang.serialization.Codec;
import java.util.Optional;
import net.minecraft.network.RegistryFriendlyByteBuf;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.world.item.ItemStack;
public class MerchantOffer {
    private final ItemCost baseCostA;
    private final Optional<ItemCost> costB;
    private final ItemStack result;
    private int uses;
    private final int maxUses;
    private final boolean rewardExp;
    private int specialPriceDiff;
    private int demand;
    private final float priceMultiplier;
    private final int xp;
    public static final Codec<MerchantOffer> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.codec(net.minecraft.world.item.trading.MerchantOffer.class, "CODEC");
    public static final StreamCodec<RegistryFriendlyByteBuf, MerchantOffer> STREAM_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.streamCodec(net.minecraft.world.item.trading.MerchantOffer.class, "STREAM_CODEC");
    private MerchantOffer(ItemCost baseCostA, Optional<ItemCost> costB, ItemStack result, int uses, int maxUses, boolean rewardExp, int specialPriceDiff, int demand, float priceMultiplier, int xp) {
        this.baseCostA = baseCostA;
        this.costB = costB;
        this.result = result;
        this.uses = uses;
        this.maxUses = maxUses;
        this.rewardExp = rewardExp;
        this.specialPriceDiff = specialPriceDiff;
        this.demand = demand;
        this.priceMultiplier = priceMultiplier;
        this.xp = xp;
    }
    public MerchantOffer(ItemCost buy, ItemStack result, int maxUses, int xp, float priceMultiplier) {
        this(buy, Optional.empty(), result, maxUses, xp, priceMultiplier);
    }
    public MerchantOffer(ItemCost baseCostA, Optional<ItemCost> costB, ItemStack result, int maxUses, int xp, float priceMultiplier) {
        this(baseCostA, costB, result, 0, maxUses, xp, priceMultiplier);
    }
    public MerchantOffer(ItemCost baseCostA, Optional<ItemCost> costB, ItemStack result, int uses, int maxUses, int xp, float priceMultiplier) {
        this(baseCostA, costB, result, uses, maxUses, xp, priceMultiplier, 0);
    }
    public MerchantOffer(ItemCost baseCostA, Optional<ItemCost> costB, ItemStack result, int uses, int maxUses, int xp, float priceMultiplier, int demand) {
        this(baseCostA, costB, result, uses, maxUses, true, 0, demand, priceMultiplier, xp);
    }
    private MerchantOffer(MerchantOffer offer) {
        this(offer.baseCostA, offer.costB, offer.result.copy(), offer.uses, offer.maxUses, offer.rewardExp,
            offer.specialPriceDiff, offer.demand, offer.priceMultiplier, offer.xp);
    }
    public ItemStack getBaseCostA() {
        return baseCostA.itemStack().copy();
    }
    public ItemStack getCostA() {
        int count = baseCostA.count();
        int price = Math.max(0, (int) Math.floor(count * demand * priceMultiplier));
        return baseCostA.itemStack().copyWithCount(Math.max(1, Math.min(baseCostA.itemStack().getMaxStackSize(), count + price + specialPriceDiff)));
    }
    public ItemStack getCostB() {
        return costB.map(cost -> cost.itemStack().copy()).orElse(ItemStack.EMPTY);
    }
    public ItemCost getItemCostA() {
        return baseCostA;
    }
    public Optional<ItemCost> getItemCostB() {
        return costB;
    }
    public ItemStack getResult() {
        return result;
    }
    public void updateDemand() {
        demand = Math.max(0, demand + uses - (maxUses - uses));
    }
    public ItemStack assemble() {
        return result.copy();
    }
    public int getUses() {
        return uses;
    }
    public void resetUses() {
        uses = 0;
    }
    public int getMaxUses() {
        return maxUses;
    }
    public void increaseUses() {
        uses++;
    }
    public int getDemand() {
        return demand;
    }
    public void addToSpecialPriceDiff(int add) {
        specialPriceDiff += add;
    }
    public void resetSpecialPriceDiff() {
        specialPriceDiff = 0;
    }
    public int getSpecialPriceDiff() {
        return specialPriceDiff;
    }
    public void setSpecialPriceDiff(int value) {
        specialPriceDiff = value;
    }
    public float getPriceMultiplier() {
        return priceMultiplier;
    }
    public int getXp() {
        return xp;
    }
    public boolean isOutOfStock() {
        return uses >= maxUses;
    }
    public void setToOutOfStock() {
        uses = maxUses;
    }
    public boolean needsRestock() {
        return uses > 0;
    }
    public boolean shouldRewardExp() {
        return rewardExp;
    }
    public boolean satisfiedBy(ItemStack buyA, ItemStack buyB) {
        return baseCostA.test(buyA) && buyA.getCount() >= getCostA().getCount()
            && (costB.isEmpty() ? buyB.isEmpty() : costB.get().test(buyB) && buyB.getCount() >= getCostB().getCount());
    }
    public boolean take(ItemStack buyA, ItemStack buyB) {
        if (!satisfiedBy(buyA, buyB)) {
            return false;
        }
        buyA.shrink(getCostA().getCount());
        if (!getCostB().isEmpty()) {
            buyB.shrink(getCostB().getCount());
        }
        return true;
    }
    public MerchantOffer copy() {
        return new MerchantOffer(this);
    }
    public static MerchantOffer createFromStream(RegistryFriendlyByteBuf input) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.item.trading.MerchantOffer#createFromStream");
    }
}
