// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.storage.loot.functions;
import com.mojang.serialization.MapCodec;
import java.util.Optional;
import net.minecraft.core.Holder;
import net.minecraft.world.item.ItemStack;
import net.minecraft.world.level.storage.loot.LootContext;
import net.minecraft.world.level.storage.loot.ValidationContext;
import net.minecraft.world.level.storage.loot.predicates.LootItemCondition;
import net.minecraft.world.level.storage.loot.providers.number.ints.ContextIntProvider;
public class SetItemCountFunction extends LootItemConditionalFunction {
    public static final MapCodec<SetItemCountFunction> MAP_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(net.minecraft.world.level.storage.loot.functions.SetItemCountFunction.class, "MAP_CODEC");
    private SetItemCountFunction(Optional<Holder<LootItemCondition>> condition, Holder<ContextIntProvider> count, boolean add) {
        super(condition);
        this.count = count;
        this.add = add;
    }
    private final Holder<ContextIntProvider> count;
    private final boolean add;
    protected void writeFunction(StringBuilder out) {
        out.append("\"type\":\"minecraft:set_count\",\"count\":");
        org.basaltmc.bukkit.loot.LootJson.value(out, count);
        out.append(",\"add\":").append(add);
    }
    public MapCodec<SetItemCountFunction> codec() {
        return MAP_CODEC;
    }
    public void validate(ValidationContext context) {
        return;
    }
    public ItemStack run(ItemStack itemStack, LootContext context) {
        int amount = count.value().getInt(context);
        itemStack.setCount(add ? itemStack.getCount() + amount : amount);
        return itemStack;
    }
    public static LootItemConditionalFunction.Builder<?> setCount(Holder<ContextIntProvider> count) {
        return setCount(count, false);
    }
    public static LootItemConditionalFunction.Builder<?> setCount(Holder<ContextIntProvider> count, boolean add) {
        return simpleBuilder(condition -> new SetItemCountFunction(condition, count, add));
    }
}
