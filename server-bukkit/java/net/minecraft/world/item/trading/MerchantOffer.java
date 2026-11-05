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
    public static final StreamCodec<RegistryFriendlyByteBuf, MerchantOffer> STREAM_CODEC = StreamCodec.of(MerchantOffer::writeToStream, MerchantOffer::createFromStream);
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
        ItemCost buy = ItemCost.STREAM_CODEC.decode(input);
        ItemStack sell = ItemStack.STREAM_CODEC.decode(input);
        Optional<ItemCost> buyB = ItemCost.OPTIONAL_STREAM_CODEC.decode(input);
        boolean exhausted = input.readBoolean();
        int uses = input.readInt();
        int maxUses = input.readInt();
        int xp = input.readInt();
        int specialPriceDiff = input.readInt();
        float priceMultiplier = input.readFloat();
        int demand = input.readInt();
        MerchantOffer offer = new MerchantOffer(buy, buyB, sell, uses, maxUses, xp, priceMultiplier, demand);
        if (exhausted) {
            offer.setToOutOfStock();
        }
        offer.setSpecialPriceDiff(specialPriceDiff);
        return offer;
    }
    private static void writeToStream(RegistryFriendlyByteBuf output, MerchantOffer offer) {
        ItemCost.STREAM_CODEC.encode(output, offer.getItemCostA());
        ItemStack.STREAM_CODEC.encode(output, offer.getResult());
        ItemCost.OPTIONAL_STREAM_CODEC.encode(output, offer.getItemCostB());
        output.writeBoolean(offer.isOutOfStock());
        output.writeInt(offer.getUses());
        output.writeInt(offer.getMaxUses());
        output.writeInt(offer.getXp());
        output.writeInt(offer.getSpecialPriceDiff());
        output.writeFloat(offer.getPriceMultiplier());
        output.writeInt(offer.getDemand());
    }
}
