// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.storage.loot.providers.number.ints;
import com.mojang.serialization.Codec;
import net.minecraft.core.Holder;
import net.minecraft.core.HolderSet;
import net.minecraft.data.worldgen.BootstrapContext;
import net.minecraft.resources.ResourceKey;
import net.minecraft.util.random.WeightedList;
import net.minecraft.world.level.storage.loot.LootContext;
import net.minecraft.world.level.storage.loot.providers.number.floats.ContextFloatProvider;
public class ContextIntProviders {
    public static final Codec<ContextIntProvider> DIRECT_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.codec(net.minecraft.world.level.storage.loot.providers.number.ints.ContextIntProviders.class, "DIRECT_CODEC");
    public static final Codec<Holder<ContextIntProvider>> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.codec(net.minecraft.world.level.storage.loot.providers.number.ints.ContextIntProviders.class, "CODEC");
    public static final Codec<HolderSet<ContextIntProvider>> LIST_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.codec(net.minecraft.world.level.storage.loot.providers.number.ints.ContextIntProviders.class, "LIST_CODEC");
    public static final ResourceKey<ContextIntProvider> COMPOSTABLE_LOW = null;
    public static final ResourceKey<ContextIntProvider> COMPOSTABLE_LOW_MEDIUM = null;
    public static final ResourceKey<ContextIntProvider> COMPOSTABLE_MEDIUM = null;
    public static final ResourceKey<ContextIntProvider> COMPOSTABLE_MEDIUM_HIGH = null;
    public static final ResourceKey<ContextIntProvider> COMPOSTABLE_ALWAYS_ADD_ONE = null;
    public static final ResourceKey<ContextIntProvider> COOKING_TIME_BAMBOO = null;
    public static final ResourceKey<ContextIntProvider> COOKING_TIME_WOOL_SLABS = null;
    public static final ResourceKey<ContextIntProvider> COOKING_TIME_WOOL_CARPETS = null;
    public static final ResourceKey<ContextIntProvider> COOKING_TIME_DRY_PLANTS = null;
    public static final ResourceKey<ContextIntProvider> COOKING_TIME_WOOD_ITEMS_EXTRA_SMALL = null;
    public static final ResourceKey<ContextIntProvider> COOKING_TIME_WOOL = null;
    public static final ResourceKey<ContextIntProvider> COOKING_TIME_WOOD_SLABS = null;
    public static final ResourceKey<ContextIntProvider> COOKING_TIME_WOOD_ITEMS_LARGE = null;
    public static final ResourceKey<ContextIntProvider> COOKING_TIME_WOOD_ITEMS_SMALL = null;
    public static final ResourceKey<ContextIntProvider> COOKING_TIME_ROOTS = null;
    public static final ResourceKey<ContextIntProvider> COOKING_TIME_WOOD_BLOCKS = null;
    public static final ResourceKey<ContextIntProvider> COOKING_TIME_HANGING_SIGNS = null;
    public static final ResourceKey<ContextIntProvider> COOKING_TIME_BOATS = null;
    public static final ResourceKey<ContextIntProvider> COOKING_TIME_COAL = null;
    public static final ResourceKey<ContextIntProvider> COOKING_TIME_BLAZE_ROD = null;
    public static final ResourceKey<ContextIntProvider> COOKING_TIME_DRIED_KELP_BLOCK = null;
    public static final ResourceKey<ContextIntProvider> COOKING_TIME_COAL_BLOCK = null;
    public static final ResourceKey<ContextIntProvider> COOKING_TIME_LAVA_BUCKET = null;
    public static final ResourceKey<ContextIntProvider> COOKING_NORMAL_BURN_TIME_REDUCTION_FACTOR = null;
    public static final ResourceKey<ContextIntProvider> COOKING_FAST_BURN_TIME_REDUCTION_FACTOR = null;
    public static final ResourceKey<ContextIntProvider> BREWING_DEFAULT_USES = null;
    public static void bootstrap(BootstrapContext<ContextIntProvider> context) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.storage.loot.providers.number.ints.ContextIntProviders#bootstrap");
    }
    public static Holder<ContextIntProvider> fromFloat(Holder<ContextFloatProvider> input) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.storage.loot.providers.number.ints.ContextIntProviders#fromFloat");
    }
    public static Holder<ContextIntProvider> binomial(int n, float p) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.storage.loot.providers.number.ints.ContextIntProviders#binomial");
    }
    public static Holder<ContextIntProvider> exactly(int value) {
        return Holder.direct(new ConstantValue(value));
    }
    public static Holder<ContextIntProvider> fromScoreboard(LootContext.EntityTarget entityTarget, String score) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.storage.loot.providers.number.ints.ContextIntProviders#fromScoreboard");
    }
    public static Holder<ContextIntProvider> avg(Holder<ContextIntProvider>... inputs) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.storage.loot.providers.number.ints.ContextIntProviders#avg");
    }
    public static Holder<ContextIntProvider> sub(Holder<ContextIntProvider> left, Holder<ContextIntProvider> right) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.storage.loot.providers.number.ints.ContextIntProviders#sub");
    }
    public static Holder<ContextIntProvider> max(Holder<ContextIntProvider>... inputs) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.storage.loot.providers.number.ints.ContextIntProviders#max");
    }
    public static Holder<ContextIntProvider> min(Holder<ContextIntProvider>... inputs) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.storage.loot.providers.number.ints.ContextIntProviders#min");
    }
    public static Holder<ContextIntProvider> mod(Holder<ContextIntProvider> left, Holder<ContextIntProvider> right) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.storage.loot.providers.number.ints.ContextIntProviders#mod");
    }
    public static Holder<ContextIntProvider> floorMod(Holder<ContextIntProvider> left, Holder<ContextIntProvider> right) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.storage.loot.providers.number.ints.ContextIntProviders#floorMod");
    }
    public static Holder<ContextIntProvider> negate(Holder<ContextIntProvider> input) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.storage.loot.providers.number.ints.ContextIntProviders#negate");
    }
    public static Holder<ContextIntProvider> mul(Holder<ContextIntProvider>... inputs) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.storage.loot.providers.number.ints.ContextIntProviders#mul");
    }
    public static Holder<ContextIntProvider> div(Holder<ContextIntProvider> left, Holder<ContextIntProvider> right) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.storage.loot.providers.number.ints.ContextIntProviders#div");
    }
    public static Holder<ContextIntProvider> floorDiv(Holder<ContextIntProvider> left, Holder<ContextIntProvider> right) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.storage.loot.providers.number.ints.ContextIntProviders#floorDiv");
    }
    public static Holder<ContextIntProvider> add(Holder<ContextIntProvider>... inputs) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.storage.loot.providers.number.ints.ContextIntProviders#add");
    }
    public static Holder<ContextIntProvider> between(int min, int max) {
        return Holder.direct(new UniformGenerator(exactly(min), exactly(max)));
    }
    public static Holder<ContextIntProvider> weighted(WeightedList<Holder<ContextIntProvider>> cases) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.storage.loot.providers.number.ints.ContextIntProviders#weighted");
    }
}
