// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.storage.loot.functions;
import com.mojang.serialization.MapCodec;
import java.util.Optional;
import net.minecraft.core.Holder;
import net.minecraft.core.HolderSet;
import net.minecraft.world.item.Instrument;
import net.minecraft.world.item.ItemStack;
import net.minecraft.world.level.storage.loot.LootContext;
import net.minecraft.world.level.storage.loot.predicates.LootItemCondition;
public class SetInstrumentFunction extends LootItemConditionalFunction {
    public static final MapCodec<SetInstrumentFunction> MAP_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(net.minecraft.world.level.storage.loot.functions.SetInstrumentFunction.class, "MAP_CODEC");
    private final HolderSet<Instrument> options;
    private SetInstrumentFunction(Optional<Holder<LootItemCondition>> condition, HolderSet<Instrument> options) {
        super(condition);
        this.options = options;
    }
    protected void writeFunction(StringBuilder out) {
        out.append("\"type\":\"minecraft:set_instrument\",\"options\":");
        org.basaltmc.bukkit.loot.LootJson.holderSet(out, options);
    }
    public MapCodec<SetInstrumentFunction> codec() {
        return MAP_CODEC;
    }
    public ItemStack run(ItemStack itemStack, LootContext context) {
        return org.basaltmc.bukkit.loot.LootFunctionBridge.apply(this, itemStack, context);
    }
    public static LootItemConditionalFunction.Builder<?> setInstrumentOptions(HolderSet<Instrument> options) {
        return simpleBuilder(condition -> new SetInstrumentFunction(condition, options));
    }
}
