// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.storage.loot.functions;
import com.mojang.serialization.MapCodec;
import java.util.Optional;
import net.minecraft.core.Holder;
import net.minecraft.world.item.ItemStack;
import net.minecraft.world.level.storage.loot.LootContext;
import net.minecraft.world.level.storage.loot.predicates.LootItemCondition;
import net.minecraft.world.level.storage.loot.providers.number.floats.ContextFloatProvider;
public class SetItemDamageFunction extends LootItemConditionalFunction {
    public static final MapCodec<SetItemDamageFunction> MAP_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(net.minecraft.world.level.storage.loot.functions.SetItemDamageFunction.class, "MAP_CODEC");
    private final Holder<ContextFloatProvider> damage;
    private final boolean add;
    private SetItemDamageFunction(Optional<Holder<LootItemCondition>> condition, Holder<ContextFloatProvider> damage, boolean add) {
        super(condition);
        this.damage = damage;
        this.add = add;
    }
    protected void writeFunction(StringBuilder out) {
        out.append("\"type\":\"minecraft:set_damage\",\"damage\":");
        org.basaltmc.bukkit.loot.LootJson.value(out, damage);
        out.append(",\"add\":").append(add);
    }
    public MapCodec<SetItemDamageFunction> codec() {
        return MAP_CODEC;
    }
    public ItemStack run(ItemStack itemStack, LootContext context) {
        return org.basaltmc.bukkit.loot.LootFunctionBridge.apply(this, itemStack, context);
    }
    public void validate(net.minecraft.world.level.storage.loot.ValidationContext context) {
        return;
    }
    public static LootItemConditionalFunction.Builder<?> setDamage(Holder<ContextFloatProvider> value) {
        return setDamage(value, false);
    }
    public static LootItemConditionalFunction.Builder<?> setDamage(Holder<ContextFloatProvider> value, boolean add) {
        return simpleBuilder(condition -> new SetItemDamageFunction(condition, value, add));
    }
}
