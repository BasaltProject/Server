// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.storage.loot.functions;
import com.mojang.serialization.MapCodec;
import java.util.Optional;
import net.minecraft.core.Holder;
import net.minecraft.world.item.ItemStack;
import net.minecraft.world.level.storage.loot.IntLimit;
import net.minecraft.world.level.storage.loot.LootContext;
import net.minecraft.world.level.storage.loot.ValidationContext;
import net.minecraft.world.level.storage.loot.predicates.LootItemCondition;
public class LimitCount extends LootItemConditionalFunction {
    private final IntLimit limit;
    public static final MapCodec<LimitCount> MAP_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(net.minecraft.world.level.storage.loot.functions.LimitCount.class, "MAP_CODEC");
    private LimitCount(Optional<Holder<LootItemCondition>> condition, IntLimit limit) {
        super(condition);
        this.limit = limit;
    }
    public MapCodec<LimitCount> codec() {
        return (com.mojang.serialization.MapCodec) MAP_CODEC;
    }
    public void validate(ValidationContext context) {
        super.validate(context);
    }
    public ItemStack run(ItemStack itemStack, LootContext context) {
        itemStack.setCount(limit.clamp(context, itemStack.getCount()));
        return itemStack;
    }
    public static LootItemConditionalFunction.Builder<?> limitCount(IntLimit limit) {
        return simpleBuilder(conditions -> new LimitCount(conditions, limit));
    }
}
