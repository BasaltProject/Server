// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.storage.loot.functions;
import net.minecraft.world.Nameable;
import net.minecraft.core.component.DataComponents;
import com.mojang.serialization.MapCodec;
import java.util.Optional;
import java.util.Set;
import net.minecraft.core.Holder;
import net.minecraft.util.context.ContextKey;
import net.minecraft.world.item.ItemStack;
import net.minecraft.world.level.storage.loot.LootContext;
import net.minecraft.world.level.storage.loot.LootContextArg;
import net.minecraft.world.level.storage.loot.predicates.LootItemCondition;
public class CopyNameFunction extends LootItemConditionalFunction {
    private final LootContextArg<Object> source;
    public static final MapCodec<CopyNameFunction> MAP_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(net.minecraft.world.level.storage.loot.functions.CopyNameFunction.class, "MAP_CODEC");
    private CopyNameFunction(Optional<Holder<LootItemCondition>> condition, LootContextArg<?> source) {
        super(condition);
        this.source = LootContextArg.cast(source);
    }
    public MapCodec<CopyNameFunction> codec() {
        return (com.mojang.serialization.MapCodec) MAP_CODEC;
    }
    public Set<ContextKey<?>> getReferencedContextParams() {
        return java.util.Set.of(source.contextParam());
    }
    public ItemStack run(ItemStack itemStack, LootContext context) {
        if (source.get(context) instanceof Nameable nameable) {
            itemStack.set(DataComponents.CUSTOM_NAME, nameable.getCustomName());
        }
        return itemStack;
    }
    public static LootItemConditionalFunction.Builder<?> copyName(LootContextArg<?> target) {
        return simpleBuilder(conditions -> new CopyNameFunction(conditions, target));
    }
}
