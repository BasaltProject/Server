// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.storage.loot.functions;
import net.minecraft.world.level.storage.loot.Validatable;
import com.mojang.serialization.MapCodec;
import java.util.Optional;
import net.minecraft.core.Holder;
import net.minecraft.world.item.ItemStack;
import net.minecraft.world.level.storage.loot.ContainerComponentManipulator;
import net.minecraft.world.level.storage.loot.LootContext;
import net.minecraft.world.level.storage.loot.ValidationContext;
import net.minecraft.world.level.storage.loot.predicates.LootItemCondition;
public class ModifyContainerContents extends LootItemConditionalFunction {
    private final ContainerComponentManipulator<?> component;
    private final Holder<LootItemFunction> modifier;
    public static final MapCodec<ModifyContainerContents> MAP_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(net.minecraft.world.level.storage.loot.functions.ModifyContainerContents.class, "MAP_CODEC");
    private ModifyContainerContents(Optional<Holder<LootItemCondition>> condition, ContainerComponentManipulator<?> component, Holder<LootItemFunction> modifier) {
        super(condition);
        this.component = component;
        this.modifier = modifier;
    }
    public MapCodec<ModifyContainerContents> codec() {
        return (com.mojang.serialization.MapCodec) MAP_CODEC;
    }
    public ItemStack run(ItemStack itemStack, LootContext context) {
        if (itemStack.isEmpty()) {
            return itemStack;
        }
        component.modifyItems(itemStack, each -> modifier.value().apply(each, context));
        return itemStack;
    }
    public void validate(ValidationContext context) {
        super.validate(context);
        Validatable.validateHolder(context, "modifier", modifier);
    }
}
