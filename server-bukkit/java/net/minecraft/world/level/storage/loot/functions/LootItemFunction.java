// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.storage.loot.functions;
import com.mojang.serialization.MapCodec;
import java.util.Optional;
import java.util.function.BiFunction;
import java.util.function.Consumer;
import net.minecraft.core.Holder;
import net.minecraft.world.item.ItemStack;
import net.minecraft.world.level.storage.loot.LootContext;
import net.minecraft.world.level.storage.loot.LootContextUser;
public interface LootItemFunction extends LootContextUser, BiFunction<ItemStack, LootContext, ItemStack> {
    MapCodec<? extends LootItemFunction> codec();
    static Consumer<ItemStack> decorate(Optional<Holder<LootItemFunction>> maybeFunction, Consumer<ItemStack> output, LootContext context) {
        if (maybeFunction.isEmpty()) {
            return output;
        }
        LootItemFunction function = maybeFunction.get().value();
        return stack -> output.accept(function.apply(stack, context));
    }
    public interface Builder {
        LootItemFunction build();
    }
}
