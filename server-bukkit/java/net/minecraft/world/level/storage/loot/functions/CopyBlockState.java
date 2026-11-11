// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.storage.loot.functions;
import net.minecraft.core.component.DataComponents;
import net.minecraft.world.item.component.BlockItemStateProperties;
import net.minecraft.world.level.storage.loot.parameters.LootContextParams;
import net.minecraft.world.level.block.state.BlockState;
import com.mojang.serialization.MapCodec;
import java.util.List;
import java.util.Optional;
import java.util.Set;
import net.minecraft.core.Holder;
import net.minecraft.util.context.ContextKey;
import net.minecraft.world.item.ItemStack;
import net.minecraft.world.level.block.Block;
import net.minecraft.world.level.block.state.properties.Property;
import net.minecraft.world.level.storage.loot.LootContext;
import net.minecraft.world.level.storage.loot.predicates.LootItemCondition;
public class CopyBlockState extends LootItemConditionalFunction {
    private final Holder<Block> block;
    private final Set<Property<?>> properties;
    public static final MapCodec<CopyBlockState> MAP_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(net.minecraft.world.level.storage.loot.functions.CopyBlockState.class, "MAP_CODEC");
    private CopyBlockState(Optional<Holder<LootItemCondition>> condition, Holder<Block> block, Set<Property<?>> properties) {
        super(condition);
        this.block = block;
        this.properties = properties;
    }
    private CopyBlockState(Optional<Holder<LootItemCondition>> condition, Holder<Block> block, List<String> propertyNames) {
        super(condition);
        this.block = block;
        Set<Property<?>> found = new java.util.HashSet<>();
        for (String each : propertyNames) {
            Property<?> hit = block.value().getStateDefinition().getProperty(each);
            if (hit != null) {
                found.add(hit);
            }
        }
        this.properties = found;
    }
    public MapCodec<CopyBlockState> codec() {
        return (com.mojang.serialization.MapCodec) MAP_CODEC;
    }
    public Set<ContextKey<?>> getReferencedContextParams() {
        return Set.of(LootContextParams.BLOCK_STATE);
    }
    protected ItemStack run(ItemStack itemStack, LootContext context) {
        BlockState state = context.getOptional(LootContextParams.BLOCK_STATE);
        if (state != null) {
            itemStack.update(DataComponents.BLOCK_STATE, BlockItemStateProperties.EMPTY, current -> {
                BlockItemStateProperties next = current;
                for (Property<?> each : properties) {
                    if (state.hasProperty(each)) {
                        next = next.with(each, state);
                    }
                }
                return next;
            });
        }
        return itemStack;
    }
    public static Builder copyState(Block block) {
        return new Builder(block);
    }
    public static class Builder extends LootItemConditionalFunction.Builder<Builder> {
        private final Holder<Block> block;
        private final Set<Property<?>> properties = new java.util.LinkedHashSet<>();
        private Builder(Block block) {
            this.block = block.builtInRegistryHolder();
        }
        public Builder copy(Property<?> property) {
            if (!block.value().getStateDefinition().getProperties().contains(property)) {
                throw new IllegalStateException("Property " + property + " is not present on block " + block);
            }
            properties.add(property);
            return this;
        }
        protected Builder getThis() {
            return this;
        }
        public LootItemFunction build() {
            return new CopyBlockState(getCondition(), block, java.util.Set.copyOf(properties));
        }
    }
}
