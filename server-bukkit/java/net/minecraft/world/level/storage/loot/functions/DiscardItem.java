// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.storage.loot.functions;
import com.mojang.serialization.MapCodec;
import java.util.Optional;
import net.minecraft.core.Holder;
import net.minecraft.world.item.ItemStack;
import net.minecraft.world.level.storage.loot.LootContext;
import net.minecraft.world.level.storage.loot.predicates.LootItemCondition;
public class DiscardItem extends LootItemConditionalFunction {
    public static final MapCodec<DiscardItem> MAP_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(net.minecraft.world.level.storage.loot.functions.DiscardItem.class, "MAP_CODEC");
    protected DiscardItem(Optional<Holder<LootItemCondition>> condition) {
        super(condition);
    }
    protected void writeFunction(StringBuilder out) {
        out.append("\"type\":\"minecraft:discard\"");
    }
    public MapCodec<DiscardItem> codec() {
        return MAP_CODEC;
    }
    protected ItemStack run(ItemStack itemStack, LootContext context) {
        return ItemStack.EMPTY;
    }
    public static LootItemConditionalFunction.Builder<?> discardItem() {
        return simpleBuilder(DiscardItem::new);
    }
}
