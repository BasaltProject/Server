// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.storage.loot.functions;
import com.mojang.serialization.MapCodec;
import java.util.Optional;
import net.minecraft.core.Holder;
import net.minecraft.core.component.DataComponentPatch;
import net.minecraft.core.component.DataComponentType;
import net.minecraft.world.item.ItemStack;
import net.minecraft.world.level.storage.loot.LootContext;
import net.minecraft.world.level.storage.loot.predicates.LootItemCondition;
public class SetComponentsFunction extends LootItemConditionalFunction {
    private final DataComponentPatch components;
    public static final MapCodec<SetComponentsFunction> MAP_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(net.minecraft.world.level.storage.loot.functions.SetComponentsFunction.class, "MAP_CODEC");
    private SetComponentsFunction(Optional<Holder<LootItemCondition>> condition, DataComponentPatch components) {
        super(condition);
        this.components = components;
    }
    public MapCodec<SetComponentsFunction> codec() {
        return (com.mojang.serialization.MapCodec) MAP_CODEC;
    }
    public ItemStack run(ItemStack itemStack, LootContext context) {
        itemStack.applyComponentsAndValidate(components);
        return itemStack;
    }
    public static <T> LootItemConditionalFunction.Builder<?> setComponent(DataComponentType<T> type, T value) {
        return simpleBuilder(conditions -> new SetComponentsFunction(conditions, DataComponentPatch.builder().set(type, value).build()));
    }
}
