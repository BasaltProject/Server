// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.storage.loot.functions;
import com.mojang.serialization.MapCodec;
import java.util.Optional;
import net.minecraft.core.Holder;
import net.minecraft.world.item.ItemStack;
import net.minecraft.world.level.storage.loot.LootContext;
import net.minecraft.world.level.storage.loot.predicates.LootItemCondition;
public class SmeltItemFunction extends LootItemConditionalFunction {
    public static final MapCodec<SmeltItemFunction> MAP_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(net.minecraft.world.level.storage.loot.functions.SmeltItemFunction.class, "MAP_CODEC");
    private final boolean useInputCount;
    private SmeltItemFunction(Optional<Holder<LootItemCondition>> condition, boolean useInputCount) {
        super(condition);
        this.useInputCount = useInputCount;
    }
    protected void writeFunction(StringBuilder out) {
        out.append("\"type\":\"minecraft:furnace_smelt\",\"use_input_count\":").append(useInputCount);
    }
    public MapCodec<SmeltItemFunction> codec() {
        return MAP_CODEC;
    }
    public ItemStack run(ItemStack itemStack, LootContext context) {
        return org.basaltmc.bukkit.loot.LootFunctionBridge.apply(this, itemStack, context);
    }
    public static LootItemConditionalFunction.Builder<?> smelted() {
        return smelted(true);
    }
    public static LootItemConditionalFunction.Builder<?> smelted(boolean useInputCount) {
        return simpleBuilder(condition -> new SmeltItemFunction(condition, useInputCount));
    }
}
