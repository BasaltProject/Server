// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.storage.loot.functions;
import com.mojang.serialization.Codec;
import com.mojang.serialization.MapCodec;
import java.util.List;
import java.util.Optional;
import net.minecraft.core.Holder;
import net.minecraft.core.HolderSet;
import net.minecraft.world.item.ItemStack;
import net.minecraft.world.level.storage.loot.LootContext;
import net.minecraft.world.level.storage.loot.ValidationContext;
import net.minecraft.world.level.storage.loot.predicates.LootItemCondition;
public class SequenceFunction extends LootItemConditionalFunction {
    private final HolderSet<LootItemFunction> functions;
    public static final MapCodec<SequenceFunction> MAP_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(net.minecraft.world.level.storage.loot.functions.SequenceFunction.class, "MAP_CODEC");
    public static final Codec<SequenceFunction> INLINE_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.codec(net.minecraft.world.level.storage.loot.functions.SequenceFunction.class, "INLINE_CODEC");
    public SequenceFunction(HolderSet<LootItemFunction> functions) {
        this(Optional.empty(), functions);
    }
    public SequenceFunction(Optional<Holder<LootItemCondition>> condition, HolderSet<LootItemFunction> functions) {
        super(condition);
        this.functions = functions;
    }
    public boolean canUseInlineCodec() {
        return condition.isEmpty();
    }
    public static SequenceFunction of(List<Holder<LootItemFunction>> functions) {
        return new SequenceFunction(Optional.empty(), HolderSet.direct(functions));
    }
    public ItemStack run(ItemStack stack, LootContext context) {
        ItemStack current = stack;
        for (Holder<LootItemFunction> step : functions) {
            current = step.value().apply(current, context);
        }
        return current;
    }
    public void validate(ValidationContext output) {
    }
    public MapCodec<SequenceFunction> codec() {
        return (com.mojang.serialization.MapCodec) MAP_CODEC;
    }
}
