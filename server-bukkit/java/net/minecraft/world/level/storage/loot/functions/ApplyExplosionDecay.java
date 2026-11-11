// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.storage.loot.functions;
import com.mojang.serialization.MapCodec;
import java.util.Optional;
import net.minecraft.core.Holder;
import net.minecraft.world.item.ItemStack;
import net.minecraft.world.level.storage.loot.LootContext;
import net.minecraft.world.level.storage.loot.predicates.LootItemCondition;
public class ApplyExplosionDecay extends LootItemConditionalFunction {
    public static final MapCodec<ApplyExplosionDecay> MAP_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(net.minecraft.world.level.storage.loot.functions.ApplyExplosionDecay.class, "MAP_CODEC");
    private ApplyExplosionDecay(Optional<Holder<LootItemCondition>> condition) {
        super(condition);
    }
    protected void writeFunction(StringBuilder out) {
        out.append("\"type\":\"minecraft:explosion_decay\"");
    }
    public MapCodec<ApplyExplosionDecay> codec() {
        return MAP_CODEC;
    }
    public ItemStack run(ItemStack itemStack, LootContext context) {
        return org.basaltmc.bukkit.loot.LootFunctionBridge.apply(this, itemStack, context);
    }
    public static LootItemConditionalFunction.Builder<?> explosionDecay() {
        return simpleBuilder(ApplyExplosionDecay::new);
    }
}
