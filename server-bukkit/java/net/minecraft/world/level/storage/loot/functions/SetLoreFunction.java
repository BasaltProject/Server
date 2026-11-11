// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.storage.loot.functions;
import net.minecraft.core.component.DataComponents;
import net.minecraft.world.item.component.ItemLore;
import com.mojang.serialization.MapCodec;
import java.util.List;
import java.util.Optional;
import java.util.Set;
import net.minecraft.core.Holder;
import net.minecraft.network.chat.Component;
import net.minecraft.util.context.ContextKey;
import net.minecraft.world.item.ItemStack;
import net.minecraft.world.level.storage.loot.LootContext;
import net.minecraft.world.level.storage.loot.predicates.LootItemCondition;
public class SetLoreFunction extends LootItemConditionalFunction {
    private final List<Component> lore;
    private final ListOperation mode;
    private final Optional<LootContext.EntityTarget> resolutionContext;
    public static final MapCodec<SetLoreFunction> MAP_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(net.minecraft.world.level.storage.loot.functions.SetLoreFunction.class, "MAP_CODEC");
    public SetLoreFunction(Optional<Holder<LootItemCondition>> condition, List<Component> lore, ListOperation mode, Optional<LootContext.EntityTarget> resolutionContext) {
        super(condition);
        this.lore = List.copyOf(lore);
        this.mode = mode;
        this.resolutionContext = resolutionContext;
    }
    public MapCodec<SetLoreFunction> codec() {
        return (com.mojang.serialization.MapCodec) MAP_CODEC;
    }
    public Set<ContextKey<?>> getReferencedContextParams() {
        return resolutionContext.<Set<ContextKey<?>>>map(target -> Set.of(target.contextParam())).orElseGet(Set::of);
    }
    public ItemStack run(ItemStack itemStack, LootContext context) {
        itemStack.update(DataComponents.LORE, ItemLore.EMPTY, old -> new ItemLore(mode.apply(old.lines(), lore, ItemLore.MAX_LINES)));
        return itemStack;
    }
    public static Builder setLore() {
        return new Builder();
    }
    public static class Builder extends LootItemConditionalFunction.Builder<Builder> {
        private final List<Component> lines = new java.util.ArrayList<>();
        private ListOperation mode;
        private Optional<LootContext.EntityTarget> resolutionContext = Optional.empty();
        public Builder() {
            this.mode = ListOperation.Append.INSTANCE;
        }
        public Builder setMode(ListOperation mode) {
            this.mode = mode;
            return this;
        }
        public Builder setResolutionContext(LootContext.EntityTarget resolutionContext) {
            this.resolutionContext = Optional.of(resolutionContext);
            return this;
        }
        public Builder addLine(Component line) {
            lines.add(line);
            return this;
        }
        protected Builder getThis() {
            return this;
        }
        public LootItemFunction build() {
            return new SetLoreFunction(getCondition(), lines, mode, resolutionContext);
        }
    }
}
