// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.storage.loot.functions;
import com.mojang.serialization.MapCodec;
import java.util.Optional;
import net.minecraft.core.Holder;
import net.minecraft.world.item.Item;
import net.minecraft.world.item.ItemStack;
import net.minecraft.world.level.storage.loot.LootContext;
import net.minecraft.world.level.storage.loot.predicates.LootItemCondition;
public class SetItemFunction extends LootItemConditionalFunction {
    private final Holder<Item> item;
    public static final MapCodec<SetItemFunction> MAP_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(net.minecraft.world.level.storage.loot.functions.SetItemFunction.class, "MAP_CODEC");
    private SetItemFunction(Optional<Holder<LootItemCondition>> condition, Holder<Item> item) {
        super(condition);
        this.item = item;
    }
    public MapCodec<SetItemFunction> codec() {
        return (com.mojang.serialization.MapCodec) MAP_CODEC;
    }
    public ItemStack run(ItemStack itemStack, LootContext context) {
        return itemStack.transmuteCopy(item.value());
    }
}
