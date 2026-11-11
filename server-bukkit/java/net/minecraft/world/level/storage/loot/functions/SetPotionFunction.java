// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.storage.loot.functions;
import com.mojang.serialization.MapCodec;
import java.util.Optional;
import net.minecraft.core.Holder;
import net.minecraft.world.item.ItemStack;
import net.minecraft.world.item.alchemy.Potion;
import net.minecraft.world.level.storage.loot.LootContext;
import net.minecraft.world.level.storage.loot.predicates.LootItemCondition;
public class SetPotionFunction extends LootItemConditionalFunction {
    public static final MapCodec<SetPotionFunction> MAP_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(net.minecraft.world.level.storage.loot.functions.SetPotionFunction.class, "MAP_CODEC");
    private final Holder<Potion> potion;
    private SetPotionFunction(Optional<Holder<LootItemCondition>> condition, Holder<Potion> potion) {
        super(condition);
        this.potion = potion;
    }
    protected void writeFunction(StringBuilder out) {
        out.append("\"type\":\"minecraft:set_potion\",\"id\":");
        org.basaltmc.bukkit.loot.LootJson.string(out, potion.getRegisteredName());
    }
    public MapCodec<SetPotionFunction> codec() {
        return MAP_CODEC;
    }
    public ItemStack run(ItemStack itemStack, LootContext context) {
        return org.basaltmc.bukkit.loot.LootFunctionBridge.apply(this, itemStack, context);
    }
    public static LootItemConditionalFunction.Builder<?> setPotion(Holder<Potion> value) {
        return simpleBuilder(condition -> new SetPotionFunction(condition, value));
    }
}
