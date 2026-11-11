// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.storage.loot.functions;
import java.util.List;
import java.util.function.Function;
import net.minecraft.core.component.DataComponents;
import net.minecraft.world.item.component.CustomModelData;
import net.minecraft.world.level.storage.loot.Validatable;
import com.mojang.serialization.MapCodec;
import java.util.Optional;
import net.minecraft.core.Holder;
import net.minecraft.world.item.ItemStack;
import net.minecraft.world.level.storage.loot.LootContext;
import net.minecraft.world.level.storage.loot.ValidationContext;
import net.minecraft.world.level.storage.loot.predicates.LootItemCondition;
import net.minecraft.world.level.storage.loot.providers.number.floats.ContextFloatProvider;
import net.minecraft.world.level.storage.loot.providers.number.ints.ContextIntProvider;
public class SetCustomModelDataFunction extends LootItemConditionalFunction {
    private final Optional<ListOperation.StandAlone<Holder<ContextFloatProvider>>> floats;
    private final Optional<ListOperation.StandAlone<Boolean>> flags;
    private final Optional<ListOperation.StandAlone<String>> strings;
    private final Optional<ListOperation.StandAlone<Holder<ContextIntProvider>>> colors;
    private static <T> List<T> merge(Optional<ListOperation.StandAlone<T>> operation, List<T> current) {
        return operation.map(o -> o.apply(current)).orElse(current);
    }
    private static <T, E> List<E> merge(Optional<ListOperation.StandAlone<T>> operation, List<E> current, Function<T, E> mapper) {
        return operation.map(o -> o.operation().apply(current, o.value().stream().map(mapper).toList())).orElse(current);
    }
    public static final MapCodec<SetCustomModelDataFunction> MAP_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(net.minecraft.world.level.storage.loot.functions.SetCustomModelDataFunction.class, "MAP_CODEC");
    public SetCustomModelDataFunction(Optional<Holder<LootItemCondition>> condition, Optional<ListOperation.StandAlone<Holder<ContextFloatProvider>>> floats, Optional<ListOperation.StandAlone<Boolean>> flags, Optional<ListOperation.StandAlone<String>> strings, Optional<ListOperation.StandAlone<Holder<ContextIntProvider>>> colors) {
        super(condition);
        this.floats = floats;
        this.flags = flags;
        this.strings = strings;
        this.colors = colors;
    }
    public void validate(ValidationContext context) {
        super.validate(context);
        floats.ifPresent(f -> Validatable.validateHolder(context, "floats", f.value()));
        colors.ifPresent(c -> Validatable.validateHolder(context, "colors", c.value()));
    }
    public MapCodec<SetCustomModelDataFunction> codec() {
        return (com.mojang.serialization.MapCodec) MAP_CODEC;
    }
    public ItemStack run(ItemStack itemStack, LootContext context) {
        CustomModelData current = itemStack.getOrDefault(DataComponents.CUSTOM_MODEL_DATA, CustomModelData.EMPTY);
        itemStack.set(DataComponents.CUSTOM_MODEL_DATA, new CustomModelData(
            merge(floats, current.floats(), provider -> provider.value().getFloat(context)),
            merge(flags, current.flags()),
            merge(strings, current.strings()),
            merge(colors, current.colors(), provider -> provider.value().getInt(context))));
        return itemStack;


    }
}
