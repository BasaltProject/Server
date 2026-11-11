// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.npc.villager;
import it.unimi.dsi.fastutil.ints.Int2ObjectOpenHashMap;
import net.minecraft.core.registries.Registries;
import net.minecraft.resources.Identifier;
import net.minecraft.sounds.SoundEvents;
import net.minecraft.tags.PoiTypeTags;
import net.minecraft.world.entity.ai.village.poi.PoiTypes;
import net.minecraft.world.item.Items;
import net.minecraft.world.item.trading.TradeSets;
import net.minecraft.world.level.block.Blocks;
import com.google.common.collect.ImmutableSet;
import it.unimi.dsi.fastutil.ints.Int2ObjectMap;
import java.util.function.Predicate;
import net.minecraft.core.Holder;
import net.minecraft.core.Registry;
import net.minecraft.network.chat.Component;
import net.minecraft.resources.ResourceKey;
import net.minecraft.sounds.SoundEvent;
import net.minecraft.world.entity.ai.village.poi.PoiType;
import net.minecraft.world.item.Item;
import net.minecraft.world.item.trading.TradeSet;
import net.minecraft.world.level.block.Block;
public record VillagerProfession(Component name, Predicate<Holder<PoiType>> heldJobSite, Predicate<Holder<PoiType>> acquirableJobSite, ImmutableSet<Item> requestedItems, ImmutableSet<Block> secondaryPoi, SoundEvent workSound, Int2ObjectMap<ResourceKey<TradeSet>> tradeSetsByLevel) {
    public static final Predicate<Holder<PoiType>> ALL_ACQUIRABLE_JOBS = holder -> holder.is(PoiTypeTags.ACQUIRABLE_JOB_SITE);
    public static final ResourceKey<VillagerProfession> NONE = key("none");
    public static final ResourceKey<VillagerProfession> ARMORER = key("armorer");
    public static final ResourceKey<VillagerProfession> BUTCHER = key("butcher");
    public static final ResourceKey<VillagerProfession> CARTOGRAPHER = key("cartographer");
    public static final ResourceKey<VillagerProfession> CLERIC = key("cleric");
    public static final ResourceKey<VillagerProfession> FARMER = key("farmer");
    public static final ResourceKey<VillagerProfession> FISHERMAN = key("fisherman");
    public static final ResourceKey<VillagerProfession> FLETCHER = key("fletcher");
    public static final ResourceKey<VillagerProfession> LEATHERWORKER = key("leatherworker");
    public static final ResourceKey<VillagerProfession> LIBRARIAN = key("librarian");
    public static final ResourceKey<VillagerProfession> MASON = key("mason");
    public static final ResourceKey<VillagerProfession> NITWIT = key("nitwit");
    public static final ResourceKey<VillagerProfession> SHEPHERD = key("shepherd");
    public static final ResourceKey<VillagerProfession> TOOLSMITH = key("toolsmith");
    public static final ResourceKey<VillagerProfession> WEAPONSMITH = key("weaponsmith");
    private static ResourceKey<VillagerProfession> key(String name) {
        return ResourceKey.create(Registries.VILLAGER_PROFESSION, Identifier.withDefaultNamespace(name));
    }
    private static VillagerProfession put(Registry<VillagerProfession> registry, ResourceKey<VillagerProfession> name, ResourceKey<PoiType> site, SoundEvent sound, String trades) {
        Predicate<Holder<PoiType>> matches = holder -> holder.is(site);
        return put(registry, name, matches, matches, ImmutableSet.of(), ImmutableSet.of(), sound, trades);
    }
    private static VillagerProfession put(Registry<VillagerProfession> registry, ResourceKey<VillagerProfession> name, Predicate<Holder<PoiType>> held, Predicate<Holder<PoiType>> acquirable,
        ImmutableSet<Item> requested, ImmutableSet<Block> secondary, SoundEvent sound, String trades) {
        Int2ObjectMap<ResourceKey<TradeSet>> byLevel = new Int2ObjectOpenHashMap<>();
        for (int level = 1; trades != null && level <= 5; level++) {
            byLevel.put(level, tradeSet(trades + "_LEVEL_" + level));
        }
        Component label = Component.translatable("entity." + name.identifier().getNamespace() + ".villager." + name.identifier().getPath());
        return Registry.register(registry, name, new VillagerProfession(label, held, acquirable, requested, secondary, sound, byLevel));
    }
    @SuppressWarnings("unchecked")
    private static ResourceKey<TradeSet> tradeSet(String field) {
        try {
            return (ResourceKey<TradeSet>) TradeSets.class.getField(field).get(null);
        } catch (ReflectiveOperationException missing) {
            throw new IllegalStateException("Unknown trade set " + field, missing);
        }
    }
    public static VillagerProfession bootstrap(Registry<VillagerProfession> registry) {
        put(registry, NONE, PoiType.NONE, ALL_ACQUIRABLE_JOBS, ImmutableSet.of(), ImmutableSet.of(), null, null);
        put(registry, NITWIT, PoiType.NONE, PoiType.NONE, ImmutableSet.of(), ImmutableSet.of(), null, null);
        put(registry, ARMORER, PoiTypes.ARMORER, SoundEvents.VILLAGER_WORK_ARMORER, "ARMORER");
        put(registry, BUTCHER, PoiTypes.BUTCHER, SoundEvents.VILLAGER_WORK_BUTCHER, "BUTCHER");
        put(registry, CARTOGRAPHER, PoiTypes.CARTOGRAPHER, SoundEvents.VILLAGER_WORK_CARTOGRAPHER, "CARTOGRAPHER");
        put(registry, CLERIC, PoiTypes.CLERIC, SoundEvents.VILLAGER_WORK_CLERIC, "CLERIC");
        put(registry, FISHERMAN, PoiTypes.FISHERMAN, SoundEvents.VILLAGER_WORK_FISHERMAN, "FISHERMAN");
        put(registry, FLETCHER, PoiTypes.FLETCHER, SoundEvents.VILLAGER_WORK_FLETCHER, "FLETCHER");
        put(registry, LEATHERWORKER, PoiTypes.LEATHERWORKER, SoundEvents.VILLAGER_WORK_LEATHERWORKER, "LEATHERWORKER");
        put(registry, LIBRARIAN, PoiTypes.LIBRARIAN, SoundEvents.VILLAGER_WORK_LIBRARIAN, "LIBRARIAN");
        put(registry, MASON, PoiTypes.MASON, SoundEvents.VILLAGER_WORK_MASON, "MASON");
        put(registry, SHEPHERD, PoiTypes.SHEPHERD, SoundEvents.VILLAGER_WORK_SHEPHERD, "SHEPHERD");
        put(registry, TOOLSMITH, PoiTypes.TOOLSMITH, SoundEvents.VILLAGER_WORK_TOOLSMITH, "TOOLSMITH");
        Predicate<Holder<PoiType>> farmer = holder -> holder.is(PoiTypes.FARMER);
        put(registry, FARMER, farmer, farmer, ImmutableSet.of(Items.WHEAT, Items.WHEAT_SEEDS, Items.BEETROOT_SEEDS, Items.BONE_MEAL),
            ImmutableSet.of(Blocks.FARMLAND), SoundEvents.VILLAGER_WORK_FARMER, "FARMER");
        return put(registry, WEAPONSMITH, PoiTypes.WEAPONSMITH, SoundEvents.VILLAGER_WORK_WEAPONSMITH, "WEAPONSMITH");
    }
    public ResourceKey<TradeSet> getTrades(int level) {
        return tradeSetsByLevel.get(level);
    }
}
