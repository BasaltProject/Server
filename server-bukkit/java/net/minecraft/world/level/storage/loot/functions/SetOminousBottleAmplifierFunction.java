// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.storage.loot.functions;
import com.mojang.serialization.MapCodec;
import java.util.Optional;
import net.minecraft.core.Holder;
import net.minecraft.world.item.ItemStack;
import net.minecraft.world.level.storage.loot.LootContext;
import net.minecraft.world.level.storage.loot.predicates.LootItemCondition;
import net.minecraft.world.level.storage.loot.providers.number.ints.ContextIntProvider;
public class SetOminousBottleAmplifierFunction extends LootItemConditionalFunction {
    public static final MapCodec<SetOminousBottleAmplifierFunction> MAP_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(net.minecraft.world.level.storage.loot.functions.SetOminousBottleAmplifierFunction.class, "MAP_CODEC");
    private final Holder<ContextIntProvider> amplifier;
    private SetOminousBottleAmplifierFunction(Optional<Holder<LootItemCondition>> condition, Holder<ContextIntProvider> amplifier) {
        super(condition);
        this.amplifier = amplifier;
    }
    protected void writeFunction(StringBuilder out) {
        out.append("\"type\":\"minecraft:set_ominous_bottle_amplifier\",\"amplifier\":");
        org.basaltmc.bukkit.loot.LootJson.value(out, amplifier);
    }
    public MapCodec<SetOminousBottleAmplifierFunction> codec() {
        return MAP_CODEC;
    }
    public ItemStack run(ItemStack itemStack, LootContext context) {
        return org.basaltmc.bukkit.loot.LootFunctionBridge.apply(this, itemStack, context);
    }
    public void validate(net.minecraft.world.level.storage.loot.ValidationContext context) {
        return;
    }
    public static LootItemConditionalFunction.Builder<?> setAmplifier(Holder<ContextIntProvider> amplifier) {
        return simpleBuilder(condition -> new SetOminousBottleAmplifierFunction(condition, amplifier));
    }
}
