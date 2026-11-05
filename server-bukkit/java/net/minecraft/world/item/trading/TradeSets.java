// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.item.trading;
import net.minecraft.core.Holder;
import net.minecraft.data.worldgen.BootstrapContext;
import net.minecraft.resources.ResourceKey;
import net.minecraft.tags.TagKey;
import net.minecraft.world.level.storage.loot.providers.number.ints.ContextIntProvider;
public class TradeSets {
    public static final ResourceKey<TradeSet> WANDERING_TRADER_BUYING = org.basaltmc.bukkit.trade.TradeKeys.set("WANDERING_TRADER_BUYING");
    public static final ResourceKey<TradeSet> WANDERING_TRADER_COMMON = org.basaltmc.bukkit.trade.TradeKeys.set("WANDERING_TRADER_COMMON");
    public static final ResourceKey<TradeSet> WANDERING_TRADER_UNCOMMON = org.basaltmc.bukkit.trade.TradeKeys.set("WANDERING_TRADER_UNCOMMON");
    public static final ResourceKey<TradeSet> ARMORER_LEVEL_1 = org.basaltmc.bukkit.trade.TradeKeys.set("ARMORER_LEVEL_1");
    public static final ResourceKey<TradeSet> ARMORER_LEVEL_2 = org.basaltmc.bukkit.trade.TradeKeys.set("ARMORER_LEVEL_2");
    public static final ResourceKey<TradeSet> ARMORER_LEVEL_3 = org.basaltmc.bukkit.trade.TradeKeys.set("ARMORER_LEVEL_3");
    public static final ResourceKey<TradeSet> ARMORER_LEVEL_4 = org.basaltmc.bukkit.trade.TradeKeys.set("ARMORER_LEVEL_4");
    public static final ResourceKey<TradeSet> ARMORER_LEVEL_5 = org.basaltmc.bukkit.trade.TradeKeys.set("ARMORER_LEVEL_5");
    public static final ResourceKey<TradeSet> BUTCHER_LEVEL_1 = org.basaltmc.bukkit.trade.TradeKeys.set("BUTCHER_LEVEL_1");
    public static final ResourceKey<TradeSet> BUTCHER_LEVEL_2 = org.basaltmc.bukkit.trade.TradeKeys.set("BUTCHER_LEVEL_2");
    public static final ResourceKey<TradeSet> BUTCHER_LEVEL_3 = org.basaltmc.bukkit.trade.TradeKeys.set("BUTCHER_LEVEL_3");
    public static final ResourceKey<TradeSet> BUTCHER_LEVEL_4 = org.basaltmc.bukkit.trade.TradeKeys.set("BUTCHER_LEVEL_4");
    public static final ResourceKey<TradeSet> BUTCHER_LEVEL_5 = org.basaltmc.bukkit.trade.TradeKeys.set("BUTCHER_LEVEL_5");
    public static final ResourceKey<TradeSet> CARTOGRAPHER_LEVEL_1 = org.basaltmc.bukkit.trade.TradeKeys.set("CARTOGRAPHER_LEVEL_1");
    public static final ResourceKey<TradeSet> CARTOGRAPHER_LEVEL_2 = org.basaltmc.bukkit.trade.TradeKeys.set("CARTOGRAPHER_LEVEL_2");
    public static final ResourceKey<TradeSet> CARTOGRAPHER_LEVEL_3 = org.basaltmc.bukkit.trade.TradeKeys.set("CARTOGRAPHER_LEVEL_3");
    public static final ResourceKey<TradeSet> CARTOGRAPHER_LEVEL_4 = org.basaltmc.bukkit.trade.TradeKeys.set("CARTOGRAPHER_LEVEL_4");
    public static final ResourceKey<TradeSet> CARTOGRAPHER_LEVEL_5 = org.basaltmc.bukkit.trade.TradeKeys.set("CARTOGRAPHER_LEVEL_5");
    public static final ResourceKey<TradeSet> CLERIC_LEVEL_1 = org.basaltmc.bukkit.trade.TradeKeys.set("CLERIC_LEVEL_1");
    public static final ResourceKey<TradeSet> CLERIC_LEVEL_2 = org.basaltmc.bukkit.trade.TradeKeys.set("CLERIC_LEVEL_2");
    public static final ResourceKey<TradeSet> CLERIC_LEVEL_3 = org.basaltmc.bukkit.trade.TradeKeys.set("CLERIC_LEVEL_3");
    public static final ResourceKey<TradeSet> CLERIC_LEVEL_4 = org.basaltmc.bukkit.trade.TradeKeys.set("CLERIC_LEVEL_4");
    public static final ResourceKey<TradeSet> CLERIC_LEVEL_5 = org.basaltmc.bukkit.trade.TradeKeys.set("CLERIC_LEVEL_5");
    public static final ResourceKey<TradeSet> FARMER_LEVEL_1 = org.basaltmc.bukkit.trade.TradeKeys.set("FARMER_LEVEL_1");
    public static final ResourceKey<TradeSet> FARMER_LEVEL_2 = org.basaltmc.bukkit.trade.TradeKeys.set("FARMER_LEVEL_2");
    public static final ResourceKey<TradeSet> FARMER_LEVEL_3 = org.basaltmc.bukkit.trade.TradeKeys.set("FARMER_LEVEL_3");
    public static final ResourceKey<TradeSet> FARMER_LEVEL_4 = org.basaltmc.bukkit.trade.TradeKeys.set("FARMER_LEVEL_4");
    public static final ResourceKey<TradeSet> FARMER_LEVEL_5 = org.basaltmc.bukkit.trade.TradeKeys.set("FARMER_LEVEL_5");
    public static final ResourceKey<TradeSet> FISHERMAN_LEVEL_1 = org.basaltmc.bukkit.trade.TradeKeys.set("FISHERMAN_LEVEL_1");
    public static final ResourceKey<TradeSet> FISHERMAN_LEVEL_2 = org.basaltmc.bukkit.trade.TradeKeys.set("FISHERMAN_LEVEL_2");
    public static final ResourceKey<TradeSet> FISHERMAN_LEVEL_3 = org.basaltmc.bukkit.trade.TradeKeys.set("FISHERMAN_LEVEL_3");
    public static final ResourceKey<TradeSet> FISHERMAN_LEVEL_4 = org.basaltmc.bukkit.trade.TradeKeys.set("FISHERMAN_LEVEL_4");
    public static final ResourceKey<TradeSet> FISHERMAN_LEVEL_5 = org.basaltmc.bukkit.trade.TradeKeys.set("FISHERMAN_LEVEL_5");
    public static final ResourceKey<TradeSet> FLETCHER_LEVEL_1 = org.basaltmc.bukkit.trade.TradeKeys.set("FLETCHER_LEVEL_1");
    public static final ResourceKey<TradeSet> FLETCHER_LEVEL_2 = org.basaltmc.bukkit.trade.TradeKeys.set("FLETCHER_LEVEL_2");
    public static final ResourceKey<TradeSet> FLETCHER_LEVEL_3 = org.basaltmc.bukkit.trade.TradeKeys.set("FLETCHER_LEVEL_3");
    public static final ResourceKey<TradeSet> FLETCHER_LEVEL_4 = org.basaltmc.bukkit.trade.TradeKeys.set("FLETCHER_LEVEL_4");
    public static final ResourceKey<TradeSet> FLETCHER_LEVEL_5 = org.basaltmc.bukkit.trade.TradeKeys.set("FLETCHER_LEVEL_5");
    public static final ResourceKey<TradeSet> LEATHERWORKER_LEVEL_1 = org.basaltmc.bukkit.trade.TradeKeys.set("LEATHERWORKER_LEVEL_1");
    public static final ResourceKey<TradeSet> LEATHERWORKER_LEVEL_2 = org.basaltmc.bukkit.trade.TradeKeys.set("LEATHERWORKER_LEVEL_2");
    public static final ResourceKey<TradeSet> LEATHERWORKER_LEVEL_3 = org.basaltmc.bukkit.trade.TradeKeys.set("LEATHERWORKER_LEVEL_3");
    public static final ResourceKey<TradeSet> LEATHERWORKER_LEVEL_4 = org.basaltmc.bukkit.trade.TradeKeys.set("LEATHERWORKER_LEVEL_4");
    public static final ResourceKey<TradeSet> LEATHERWORKER_LEVEL_5 = org.basaltmc.bukkit.trade.TradeKeys.set("LEATHERWORKER_LEVEL_5");
    public static final ResourceKey<TradeSet> LIBRARIAN_LEVEL_1 = org.basaltmc.bukkit.trade.TradeKeys.set("LIBRARIAN_LEVEL_1");
    public static final ResourceKey<TradeSet> LIBRARIAN_LEVEL_2 = org.basaltmc.bukkit.trade.TradeKeys.set("LIBRARIAN_LEVEL_2");
    public static final ResourceKey<TradeSet> LIBRARIAN_LEVEL_3 = org.basaltmc.bukkit.trade.TradeKeys.set("LIBRARIAN_LEVEL_3");
    public static final ResourceKey<TradeSet> LIBRARIAN_LEVEL_4 = org.basaltmc.bukkit.trade.TradeKeys.set("LIBRARIAN_LEVEL_4");
    public static final ResourceKey<TradeSet> LIBRARIAN_LEVEL_5 = org.basaltmc.bukkit.trade.TradeKeys.set("LIBRARIAN_LEVEL_5");
    public static final ResourceKey<TradeSet> MASON_LEVEL_1 = org.basaltmc.bukkit.trade.TradeKeys.set("MASON_LEVEL_1");
    public static final ResourceKey<TradeSet> MASON_LEVEL_2 = org.basaltmc.bukkit.trade.TradeKeys.set("MASON_LEVEL_2");
    public static final ResourceKey<TradeSet> MASON_LEVEL_3 = org.basaltmc.bukkit.trade.TradeKeys.set("MASON_LEVEL_3");
    public static final ResourceKey<TradeSet> MASON_LEVEL_4 = org.basaltmc.bukkit.trade.TradeKeys.set("MASON_LEVEL_4");
    public static final ResourceKey<TradeSet> MASON_LEVEL_5 = org.basaltmc.bukkit.trade.TradeKeys.set("MASON_LEVEL_5");
    public static final ResourceKey<TradeSet> SHEPHERD_LEVEL_1 = org.basaltmc.bukkit.trade.TradeKeys.set("SHEPHERD_LEVEL_1");
    public static final ResourceKey<TradeSet> SHEPHERD_LEVEL_2 = org.basaltmc.bukkit.trade.TradeKeys.set("SHEPHERD_LEVEL_2");
    public static final ResourceKey<TradeSet> SHEPHERD_LEVEL_3 = org.basaltmc.bukkit.trade.TradeKeys.set("SHEPHERD_LEVEL_3");
    public static final ResourceKey<TradeSet> SHEPHERD_LEVEL_4 = org.basaltmc.bukkit.trade.TradeKeys.set("SHEPHERD_LEVEL_4");
    public static final ResourceKey<TradeSet> SHEPHERD_LEVEL_5 = org.basaltmc.bukkit.trade.TradeKeys.set("SHEPHERD_LEVEL_5");
    public static final ResourceKey<TradeSet> TOOLSMITH_LEVEL_1 = org.basaltmc.bukkit.trade.TradeKeys.set("TOOLSMITH_LEVEL_1");
    public static final ResourceKey<TradeSet> TOOLSMITH_LEVEL_2 = org.basaltmc.bukkit.trade.TradeKeys.set("TOOLSMITH_LEVEL_2");
    public static final ResourceKey<TradeSet> TOOLSMITH_LEVEL_3 = org.basaltmc.bukkit.trade.TradeKeys.set("TOOLSMITH_LEVEL_3");
    public static final ResourceKey<TradeSet> TOOLSMITH_LEVEL_4 = org.basaltmc.bukkit.trade.TradeKeys.set("TOOLSMITH_LEVEL_4");
    public static final ResourceKey<TradeSet> TOOLSMITH_LEVEL_5 = org.basaltmc.bukkit.trade.TradeKeys.set("TOOLSMITH_LEVEL_5");
    public static final ResourceKey<TradeSet> WEAPONSMITH_LEVEL_1 = org.basaltmc.bukkit.trade.TradeKeys.set("WEAPONSMITH_LEVEL_1");
    public static final ResourceKey<TradeSet> WEAPONSMITH_LEVEL_2 = org.basaltmc.bukkit.trade.TradeKeys.set("WEAPONSMITH_LEVEL_2");
    public static final ResourceKey<TradeSet> WEAPONSMITH_LEVEL_3 = org.basaltmc.bukkit.trade.TradeKeys.set("WEAPONSMITH_LEVEL_3");
    public static final ResourceKey<TradeSet> WEAPONSMITH_LEVEL_4 = org.basaltmc.bukkit.trade.TradeKeys.set("WEAPONSMITH_LEVEL_4");
    public static final ResourceKey<TradeSet> WEAPONSMITH_LEVEL_5 = org.basaltmc.bukkit.trade.TradeKeys.set("WEAPONSMITH_LEVEL_5");
    public static void bootstrap(BootstrapContext<TradeSet> context) {
        org.basaltmc.bukkit.facade.Bootstraps.replay(context, net.minecraft.core.registries.Registries.TRADE_SET, TradeSets.class);
    }
    public static Holder.Reference<TradeSet> register(BootstrapContext<TradeSet> context, ResourceKey<TradeSet> resourceKey, TagKey<VillagerTrade> tradeTag) {
        return register(context, resourceKey, tradeTag, net.minecraft.world.level.storage.loot.providers.number.ints.ContextIntProviders.exactly(2));
    }
    public static Holder.Reference<TradeSet> register(BootstrapContext<TradeSet> context, ResourceKey<TradeSet> resourceKey, TagKey<VillagerTrade> tradeTag, Holder<ContextIntProvider> amount) {
        return context.register(resourceKey, new TradeSet(context.<VillagerTrade>lookup(net.minecraft.core.registries.Registries.VILLAGER_TRADE).getOrThrow(tradeTag),
            amount, false, java.util.Optional.of(resourceKey.identifier().withPrefix("trade_set/"))));
    }
    public static ResourceKey<TradeSet> resourceKey(String path) {
        return ResourceKey.create(net.minecraft.core.registries.Registries.TRADE_SET, net.minecraft.resources.Identifier.withDefaultNamespace(path));
    }
}
