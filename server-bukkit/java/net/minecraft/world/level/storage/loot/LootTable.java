// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.storage.loot;
import com.mojang.serialization.Codec;
import it.unimi.dsi.fastutil.objects.ObjectArrayList;
import java.util.List;
import java.util.Optional;
import java.util.function.Consumer;
import net.minecraft.core.Holder;
import net.minecraft.core.HolderSet;
import net.minecraft.resources.Identifier;
import net.minecraft.resources.ResourceKey;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.util.RandomSource;
import net.minecraft.util.context.ContextKeySet;
import net.minecraft.world.Container;
import net.minecraft.world.item.ItemStack;
import net.minecraft.world.level.storage.loot.functions.FunctionUserBuilder;
import org.basaltmc.bukkit.loot.LootJson;
import net.minecraft.world.level.storage.loot.functions.LootItemFunction;
public class LootTable implements Validatable, LootJson.Node {
    private Identifier key;
    private final List<LootPool> pools;
    private final Optional<Holder<LootItemFunction>> modifier;

    public static LootTable of(Identifier key) {
        LootTable table = new LootTable(null, Optional.empty(), null, Optional.empty());
        table.key = key;
        return table;
    }

    public Identifier key() {
        return key;
    }

    private void roll(LootParams params, long seed, Consumer<ItemStack> output) {
        if (key == null && pools == null) {
            return;
        }
        rollNative(key == null ? null : key.toString(), key == null ? LootJson.of(this) : null, params, seed, output);
    }

    static void rollNative(String table, String json, LootParams params, long seed, Consumer<ItemStack> output) {
        net.minecraft.world.phys.Vec3 origin = params.contextMap().getOrDefault(
            net.minecraft.world.level.storage.loot.parameters.LootContextParams.ORIGIN,
            net.minecraft.world.phys.Vec3.ZERO);
        String world = params.getLevel().getWorld().getName();
        String[] flat = json == null
            ? org.basaltmc.bukkit.MiscNatives.lootRoll(table, world, origin.x, origin.y, origin.z, params.getLuck(), seed, null)
            : org.basaltmc.bukkit.MiscNatives.lootRollJson(json, world, origin.x, origin.y, origin.z, params.getLuck(), seed, null);
        if (flat == null) {
            return;
        }
        for (int i = 0; i + 2 < flat.length; i += 3) {
            net.minecraft.world.item.Item item = net.minecraft.core.registries.BuiltInRegistries.ITEM.getOptional(
                Identifier.parse(flat[i])).orElse(null);
            if (item != null) {
                output.accept(new ItemStack(item, Integer.parseInt(flat[i + 1])));
            }
        }
    }

    @Override
    public void writeJson(StringBuilder out) {
        if (pools == null) {
            LootJson.string(out, String.valueOf(key));
            return;
        }
        out.append("{\"pools\":");
        LootJson.nodes(out, pools);
        LootJson.optional(out, "modifier", modifier);
        out.append('}');
    }

    public static final Codec<ResourceKey<LootTable>> KEY_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.codec(net.minecraft.world.level.storage.loot.LootTable.class, "KEY_CODEC");
    public static final ContextKeySet DEFAULT_PARAM_SET = net.minecraft.world.level.storage.loot.parameters.LootContextParamSets.ALL_PARAMS;
    public static final long RANDOMIZE_SEED = 0L;
    public static final Codec<LootTable> DIRECT_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.codec(net.minecraft.world.level.storage.loot.LootTable.class, "DIRECT_CODEC");
    public static final Codec<Holder<LootTable>> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.codec(net.minecraft.world.level.storage.loot.LootTable.class, "CODEC");
    public static final Codec<HolderSet<LootTable>> LIST_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.codec(net.minecraft.world.level.storage.loot.LootTable.class, "LIST_CODEC");
    public static final LootTable EMPTY = null;
    private LootTable(ContextKeySet paramSet, Optional<Identifier> randomSequence, List<LootPool> pools, Optional<Holder<LootItemFunction>> modifier) {
        this.pools = pools;
        this.modifier = modifier;
    }
    public static Consumer<ItemStack> createStackSplitter(ServerLevel level, Consumer<ItemStack> output) {
        return stack -> {
            int max = stack.getMaxStackSize();
            if (stack.getCount() <= max) {
                output.accept(stack);
                return;
            }
            int left = stack.getCount();
            while (left > 0) {
                ItemStack part = stack.copyWithCount(Math.min(max, left));
                left -= part.getCount();
                output.accept(part);
            }
        };
    }
    public void getRandomItemsRaw(LootParams params, Consumer<ItemStack> output) {
        roll(params, 0L, output);
    }
    public void getRandomItemsRaw(LootContext context, Consumer<ItemStack> output) {
        roll(context.getParams(), 0L, output);
    }
    public void getRandomItems(LootParams params, long optionalLootTableSeed, Consumer<ItemStack> output) {
        roll(params, optionalLootTableSeed, createStackSplitter(params.getLevel(), output));
    }
    public void getRandomItems(LootParams params, Consumer<ItemStack> output) {
        getRandomItems(params, 0L, output);
    }
    public void getRandomItems(LootContext context, Consumer<ItemStack> output) {
        roll(context.getParams(), context.getRandom().nextLong(), createStackSplitter(context.getLevel(), output));
    }
    public ObjectArrayList<ItemStack> getRandomItems(LootParams params, RandomSource randomSource) {
        return getRandomItems(params, randomSource.nextLong());
    }
    public ObjectArrayList<ItemStack> getRandomItems(LootParams params, long optionalLootTableSeed) {
        ObjectArrayList<ItemStack> result = new ObjectArrayList<>();
        getRandomItems(params, optionalLootTableSeed, result::add);
        return result;
    }
    public ObjectArrayList<ItemStack> getRandomItems(LootParams params) {
        return getRandomItems(params, 0L);
    }
    public ContextKeySet getParamSet() {
        return DEFAULT_PARAM_SET;
    }
    public void validate(ValidationContext context) {

    }
    public void fill(Container container, LootParams params, long optionalRandomSeed) {
        ObjectArrayList<ItemStack> items = getRandomItems(params, optionalRandomSeed);
        for (int i = 0; i < items.size() && i < container.getContainerSize(); i++) {
            container.setItem(i, items.get(i));
        }
    }
    public static Builder lootTable() {
        return new Builder();
    }
    public static class Builder implements FunctionUserBuilder<Builder> {
        private final List<LootPool> pools = new java.util.ArrayList<>();
        private final List<Holder<LootItemFunction>> functions = new java.util.ArrayList<>();
        private ContextKeySet paramSet;
        private Identifier randomSequence;
        public Builder() {
        }
        public Builder withPool(LootPool.Builder pool) {
            pools.add(pool.build());
            return this;
        }
        public Builder setParamSet(ContextKeySet paramSet) {
            this.paramSet = paramSet;
            return this;
        }
        public Builder setRandomSequence(Identifier key) {
            this.randomSequence = key;
            return this;
        }
        public Builder apply(Holder<LootItemFunction> function) {
            functions.add(function);
            return this;
        }
        public Builder unwrap() {
            return this;
        }
        public LootTable build() {
            return new LootTable(paramSet, Optional.ofNullable(randomSequence), List.copyOf(pools),
                FunctionUserBuilder.buildFunction(functions));
        }
    }
}
