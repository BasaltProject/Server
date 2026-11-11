// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.storage.loot.functions;
import net.minecraft.core.registries.BuiltInRegistries;
import net.minecraft.core.component.TypedDataComponent;
import com.mojang.serialization.MapCodec;
import java.util.List;
import java.util.Optional;
import java.util.Set;
import net.minecraft.core.Holder;
import net.minecraft.core.component.DataComponentGetter;
import net.minecraft.core.component.DataComponentType;
import net.minecraft.util.context.ContextKey;
import net.minecraft.world.entity.Entity;
import net.minecraft.world.item.ItemStack;
import net.minecraft.world.level.block.entity.BlockEntity;
import net.minecraft.world.level.storage.loot.LootContext;
import net.minecraft.world.level.storage.loot.LootContextArg;
import net.minecraft.world.level.storage.loot.predicates.LootItemCondition;
public class CopyComponentsFunction extends LootItemConditionalFunction {
    private final LootContextArg<DataComponentGetter> source;
    private final Optional<List<DataComponentType<?>>> include;
    private final Optional<List<DataComponentType<?>>> exclude;
    public static final MapCodec<CopyComponentsFunction> MAP_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(net.minecraft.world.level.storage.loot.functions.CopyComponentsFunction.class, "MAP_CODEC");
    private CopyComponentsFunction(Optional<Holder<LootItemCondition>> condition, LootContextArg<DataComponentGetter> source, Optional<List<DataComponentType<?>>> include, Optional<List<DataComponentType<?>>> exclude) {
        super(condition);
        this.source = source;
        this.include = include.map(List::copyOf);
        this.exclude = exclude.map(List::copyOf);
    }
    public MapCodec<CopyComponentsFunction> codec() {
        return (com.mojang.serialization.MapCodec) MAP_CODEC;
    }
    public Set<ContextKey<?>> getReferencedContextParams() {
        return java.util.Set.of(source.contextParam());
    }
    public ItemStack run(ItemStack itemStack, LootContext context) {
        DataComponentGetter data = source.get(context);
        if (data == null) {
            return itemStack;
        }
        List<DataComponentType<?>> skipped = exclude.orElse(List.of());
        List<DataComponentType<?>> wanted = new java.util.ArrayList<>();
        if (include.isPresent()) {
            wanted.addAll(include.get());
        } else {
            BuiltInRegistries.DATA_COMPONENT_TYPE.listElements().forEach(ref -> wanted.add(ref.value()));
        }
        for (DataComponentType<?> type : wanted) {
            if (!skipped.contains(type)) {
                TypedDataComponent<?> typed = data.getTyped(type);
                if (typed != null) {
                    itemStack.set(typed);
                }
            }
        }
        return itemStack;
    }
    public static Builder copyComponentsFromEntity(ContextKey<? extends Entity> source) {
        return new Builder(new DirectSource(source));
    }
    public static Builder copyComponentsFromBlockEntity(ContextKey<? extends BlockEntity> source) {
        return new Builder(new BlockEntitySource(source));
    }
    public static class Builder extends LootItemConditionalFunction.Builder<Builder> {
        private final LootContextArg<DataComponentGetter> origin;
        private final List<DataComponentType<?>> listed = new java.util.ArrayList<>();
        private final List<DataComponentType<?>> dropped = new java.util.ArrayList<>();
        private boolean restrict;
        private boolean skip;
        private Builder(LootContextArg<DataComponentGetter> source) {
            this.origin = source;
            
        }
        public Builder include(DataComponentType<?> type) {
            listed.add(type);
            this.restrict = true;
            return this;
        }
        public Builder exclude(DataComponentType<?> type) {
            dropped.add(type);
            this.skip = true;
            return this;
        }
        protected Builder getThis() {
            return this;
        }
        public LootItemFunction build() {
            return new CopyComponentsFunction(getCondition(), origin, restrict ? Optional.of(List.copyOf(listed)) : Optional.empty(),
                skip ? Optional.of(List.copyOf(dropped)) : Optional.empty());
        }
    }
    static record DirectSource<T extends DataComponentGetter>(ContextKey<? extends T> contextParam) implements LootContextArg.Getter<T, DataComponentGetter> {
        public DataComponentGetter get(T value) {
            return value;
        }
    }
    static record BlockEntitySource(ContextKey<? extends BlockEntity> contextParam) implements LootContextArg.Getter<BlockEntity, DataComponentGetter> {
        public DataComponentGetter get(BlockEntity blockEntity) {
            return blockEntity.collectComponents();
        }
    }
}
