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
import net.minecraft.world.level.storage.loot.functions.LootItemFunction;
public class LootTable implements Validatable {
    private Identifier key;

    public static LootTable of(Identifier key) {
        LootTable table = new LootTable(null, Optional.empty(), List.of(), Optional.empty());
        table.key = key;
        return table;
    }

    public Identifier key() {
        return key;
    }

    private void roll(LootParams params, long seed, Consumer<ItemStack> output) {
        if (key == null) {
            return;
        }
        net.minecraft.world.phys.Vec3 origin = params.contextMap().getOrDefault(
            net.minecraft.world.level.storage.loot.parameters.LootContextParams.ORIGIN,
            net.minecraft.world.phys.Vec3.ZERO);
        String[] flat = org.basaltmc.bukkit.MiscNatives.lootRoll(key.toString(), params.getLevel().getWorld().getName(),
            origin.x, origin.y, origin.z, params.getLuck(), seed, null);
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
    public static final Codec<ResourceKey<LootTable>> KEY_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.codec(net.minecraft.world.level.storage.loot.LootTable.class, "KEY_CODEC");
    public static final ContextKeySet DEFAULT_PARAM_SET = null;
    public static final long RANDOMIZE_SEED = 0L;
    public static final Codec<LootTable> DIRECT_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.codec(net.minecraft.world.level.storage.loot.LootTable.class, "DIRECT_CODEC");
    public static final Codec<Holder<LootTable>> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.codec(net.minecraft.world.level.storage.loot.LootTable.class, "CODEC");
    public static final Codec<HolderSet<LootTable>> LIST_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.codec(net.minecraft.world.level.storage.loot.LootTable.class, "LIST_CODEC");
    public static final LootTable EMPTY = null;
    private LootTable(ContextKeySet paramSet, Optional<Identifier> randomSequence, List<LootPool> pools, Optional<Holder<LootItemFunction>> modifier) {

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
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.storage.loot.LootTable#lootTable");
    }
    public static class Builder implements FunctionUserBuilder<Builder> {
        public Builder() {
            throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.storage.loot.LootTable.Builder#<init>");
        }
        public Builder withPool(LootPool.Builder pool) {
            throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.storage.loot.LootTable.Builder#withPool");
        }
        public Builder setParamSet(ContextKeySet paramSet) {
            throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.storage.loot.LootTable.Builder#setParamSet");
        }
        public Builder setRandomSequence(Identifier key) {
            throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.storage.loot.LootTable.Builder#setRandomSequence");
        }
        public Builder apply(Holder<LootItemFunction> function) {
            throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.storage.loot.LootTable.Builder#apply");
        }
        public Builder unwrap() {
            throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.storage.loot.LootTable.Builder#unwrap");
        }
        public LootTable build() {
            throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.storage.loot.LootTable.Builder#build");
        }
    }
}
