// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.storage.loot.entries;
import net.minecraft.world.level.storage.loot.Validatable;
import com.mojang.serialization.MapCodec;
import java.util.Optional;
import java.util.function.Consumer;
import net.minecraft.core.Holder;
import net.minecraft.world.item.ItemStack;
import net.minecraft.world.item.slot.SlotSource;
import net.minecraft.world.level.storage.loot.LootContext;
import net.minecraft.world.level.storage.loot.ValidationContext;
import net.minecraft.world.level.storage.loot.functions.LootItemFunction;
import net.minecraft.world.level.storage.loot.predicates.LootItemCondition;
public class SlotLoot extends SingleEntryContainerBase {
    private final Holder<SlotSource> slotSource;
    public static final MapCodec<SlotLoot> MAP_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(net.minecraft.world.level.storage.loot.entries.SlotLoot.class, "MAP_CODEC");
    private SlotLoot(Holder<SlotSource> slotSource, int weight, int quality, Optional<Holder<LootItemCondition>> condition, Optional<Holder<LootItemFunction>> modifier) {
        super(weight, quality, condition, modifier);
        this.slotSource = slotSource;
    }
    public MapCodec<SlotLoot> codec() {
        return (com.mojang.serialization.MapCodec) MAP_CODEC;
    }
    public void createItemStack(Consumer<ItemStack> output, LootContext context) {
        slotSource.value().provide(context).itemCopies().filter(stack -> !stack.isEmpty()).forEach(output);
    }
    public void validate(ValidationContext context) {
        super.validate(context);
        Validatable.validateHolder(context, "slot_source", slotSource);


    }
}
