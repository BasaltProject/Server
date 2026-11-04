// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.item.trading;

import com.mojang.serialization.Codec;
import java.util.ArrayList;
import java.util.Collection;
import net.minecraft.network.RegistryFriendlyByteBuf;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.world.item.ItemStack;

public class MerchantOffers extends ArrayList<MerchantOffer> {
    public static final Codec<MerchantOffers> CODEC = MerchantOffer.CODEC.listOf().xmap(MerchantOffers::new,
        offers -> java.util.List.copyOf(offers));
    public static final StreamCodec<RegistryFriendlyByteBuf, MerchantOffers> STREAM_CODEC = StreamCodec.of(
        (buf, offers) -> {
            net.minecraft.network.codec.ByteBufCodecs.VAR_INT.encode(buf, offers.size());
            offers.forEach(offer -> MerchantOffer.STREAM_CODEC.encode(buf, offer));
        }, buf -> {
            int count = net.minecraft.network.codec.ByteBufCodecs.VAR_INT.decode(buf);
            MerchantOffers offers = new MerchantOffers(count);
            for (int i = 0; i < count; i++) {
                offers.add(MerchantOffer.STREAM_CODEC.decode(buf));
            }
            return offers;
        });

    public MerchantOffers() {}

    private MerchantOffers(int initialCapacity) {
        super(initialCapacity);
    }

    private MerchantOffers(Collection<MerchantOffer> offers) {
        super(offers);
    }

    public MerchantOffer getRecipeFor(ItemStack buyA, ItemStack buyB, int selectionHint) {
        if (selectionHint > 0 && selectionHint < size()) {
            MerchantOffer hinted = get(selectionHint);
            return hinted.satisfiedBy(buyA, buyB) ? hinted : null;
        }
        for (MerchantOffer offer : this) {
            if (offer.satisfiedBy(buyA, buyB)) {
                return offer;
            }
        }
        return null;
    }

    public MerchantOffers copy() {
        MerchantOffers copy = new MerchantOffers(size());
        for (MerchantOffer offer : this) {
            copy.add(offer.copy());
        }
        return copy;
    }
}
