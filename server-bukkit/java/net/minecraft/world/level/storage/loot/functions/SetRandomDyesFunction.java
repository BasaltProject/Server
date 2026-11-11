// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.storage.loot.functions;
import com.mojang.serialization.MapCodec;
import java.util.Optional;
import net.minecraft.core.Holder;
import net.minecraft.world.item.ItemStack;
import net.minecraft.world.level.storage.loot.LootContext;
import net.minecraft.world.level.storage.loot.predicates.LootItemCondition;
import net.minecraft.world.level.storage.loot.providers.number.ints.ContextIntProvider;
public class SetRandomDyesFunction extends LootItemConditionalFunction {
    public static final MapCodec<SetRandomDyesFunction> MAP_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(net.minecraft.world.level.storage.loot.functions.SetRandomDyesFunction.class, "MAP_CODEC");
    private final Holder<ContextIntProvider> numberOfDyes;
    private SetRandomDyesFunction(Optional<Holder<LootItemCondition>> condition, Holder<ContextIntProvider> numberOfDyes) {
        super(condition);
        this.numberOfDyes = numberOfDyes;
    }
    protected void writeFunction(StringBuilder out) {
        out.append("\"type\":\"minecraft:set_random_dyes\",\"number_of_dyes\":");
        org.basaltmc.bukkit.loot.LootJson.value(out, numberOfDyes);
    }
    public MapCodec<SetRandomDyesFunction> codec() {
        return MAP_CODEC;
    }
    public ItemStack run(ItemStack itemStack, LootContext context) {
        return org.basaltmc.bukkit.loot.LootFunctionBridge.apply(this, itemStack, context);
    }
    public static LootItemConditionalFunction.Builder<?> withCount(Holder<ContextIntProvider> numberOfDyes) {
        return simpleBuilder(condition -> new SetRandomDyesFunction(condition, numberOfDyes));
    }
}
