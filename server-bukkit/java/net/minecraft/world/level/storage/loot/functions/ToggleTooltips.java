// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.storage.loot.functions;
import net.minecraft.world.item.component.TooltipDisplay;
import net.minecraft.core.component.DataComponents;
import com.mojang.serialization.MapCodec;
import java.util.Map;
import java.util.Optional;
import net.minecraft.core.Holder;
import net.minecraft.core.component.DataComponentType;
import net.minecraft.world.item.ItemStack;
import net.minecraft.world.level.storage.loot.LootContext;
import net.minecraft.world.level.storage.loot.predicates.LootItemCondition;
public class ToggleTooltips extends LootItemConditionalFunction {
    private final Map<DataComponentType<?>, Boolean> values;
    public static final MapCodec<ToggleTooltips> MAP_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(net.minecraft.world.level.storage.loot.functions.ToggleTooltips.class, "MAP_CODEC");
    private ToggleTooltips(Optional<Holder<LootItemCondition>> condition, Map<DataComponentType<?>, Boolean> values) {
        super(condition);
        this.values = values;
    }
    protected ItemStack run(ItemStack itemStack, LootContext context) {
        itemStack.update(DataComponents.TOOLTIP_DISPLAY, TooltipDisplay.DEFAULT, display -> {
            TooltipDisplay result = display;
            for (Map.Entry<DataComponentType<?>, Boolean> entry : values.entrySet()) {
                result = result.withHidden(entry.getKey(), !entry.getValue());
            }
            return result;
        });
        return itemStack;


    }
    public MapCodec<ToggleTooltips> codec() {
        return (com.mojang.serialization.MapCodec) MAP_CODEC;
    }
}
