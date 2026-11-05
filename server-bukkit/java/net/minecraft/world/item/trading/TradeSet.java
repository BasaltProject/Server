// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.item.trading;
import com.mojang.serialization.Codec;
import java.util.Optional;
import net.minecraft.core.Holder;
import net.minecraft.core.HolderSet;
import net.minecraft.resources.Identifier;
import net.minecraft.world.level.storage.loot.LootContext;
import net.minecraft.world.level.storage.loot.providers.number.ints.ContextIntProvider;
public record TradeSet(HolderSet<VillagerTrade> trades, Holder<ContextIntProvider> amount, boolean allowDuplicates, Optional<Identifier> randomSequence) {
    public static final Codec<TradeSet> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.codec(net.minecraft.world.item.trading.TradeSet.class, "CODEC");
    public int calculateNumberOfTrades(LootContext lootContext) {
        return amount.value().getInt(lootContext);
    }
}
