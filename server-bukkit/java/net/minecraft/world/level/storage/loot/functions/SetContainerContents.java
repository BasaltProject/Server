// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.storage.loot.functions;
import net.minecraft.world.level.storage.loot.LootTable;
import net.minecraft.world.level.storage.loot.Validatable;
import com.mojang.serialization.MapCodec;
import java.util.List;
import java.util.Optional;
import net.minecraft.core.Holder;
import net.minecraft.world.item.ItemStack;
import net.minecraft.world.level.storage.loot.ContainerComponentManipulator;
import net.minecraft.world.level.storage.loot.LootContext;
import net.minecraft.world.level.storage.loot.ValidationContext;
import net.minecraft.world.level.storage.loot.entries.LootPoolEntryContainer;
import net.minecraft.world.level.storage.loot.predicates.LootItemCondition;
public class SetContainerContents extends LootItemConditionalFunction {
    private final ContainerComponentManipulator<?> component;
    private final List<LootPoolEntryContainer> entries;
    public static final MapCodec<SetContainerContents> MAP_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(net.minecraft.world.level.storage.loot.functions.SetContainerContents.class, "MAP_CODEC");
    private SetContainerContents(Optional<Holder<LootItemCondition>> condition, ContainerComponentManipulator<?> component, List<LootPoolEntryContainer> entries) {
        super(condition);
        this.component = component;
        this.entries = List.copyOf(entries);
    }
    public MapCodec<SetContainerContents> codec() {
        return (com.mojang.serialization.MapCodec) MAP_CODEC;
    }
    public ItemStack run(ItemStack itemStack, LootContext context) {
        if (itemStack.isEmpty()) {
            return itemStack;
        }
        java.util.List<ItemStack> gathered = new java.util.ArrayList<>();
        for (LootPoolEntryContainer container : entries) {
            container.expand(context, entry -> entry.createItemStack(LootTable.createStackSplitter(context.getLevel(), gathered::add), context));
        }
        component.setContents(itemStack, gathered.stream());
        return itemStack;
    }
    public void validate(ValidationContext context) {
        super.validate(context);
        Validatable.validate(context, "entries", entries);
    }
    public static Builder setContents(ContainerComponentManipulator<?> component) {
        return new Builder(component);
    }
    public static class Builder extends LootItemConditionalFunction.Builder<Builder> {
        private final List<LootPoolEntryContainer> collected = new java.util.ArrayList<>();
        private final ContainerComponentManipulator<?> manipulator;
        public Builder(ContainerComponentManipulator<?> component) {
            this.manipulator = component;
            
        }
        protected Builder getThis() {
            return this;
        }
        public Builder withEntry(LootPoolEntryContainer.Builder<?> entry) {
            collected.add(entry.build());
            return this;
        }
        public LootItemFunction build() {
            return new SetContainerContents(getCondition(), manipulator, collected);
        }
    }
}
