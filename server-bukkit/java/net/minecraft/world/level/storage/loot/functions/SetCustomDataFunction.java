// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.storage.loot.functions;
import net.minecraft.world.item.component.CustomData;
import net.minecraft.core.component.DataComponents;
import com.mojang.serialization.MapCodec;
import java.util.Optional;
import net.minecraft.core.Holder;
import net.minecraft.nbt.CompoundTag;
import net.minecraft.world.item.ItemStack;
import net.minecraft.world.level.storage.loot.LootContext;
import net.minecraft.world.level.storage.loot.predicates.LootItemCondition;
public class SetCustomDataFunction extends LootItemConditionalFunction {
    private final CompoundTag tag;
    public static final MapCodec<SetCustomDataFunction> MAP_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(net.minecraft.world.level.storage.loot.functions.SetCustomDataFunction.class, "MAP_CODEC");
    private SetCustomDataFunction(Optional<Holder<LootItemCondition>> condition, CompoundTag tag) {
        super(condition);
        this.tag = tag;
    }
    public MapCodec<SetCustomDataFunction> codec() {
        return (com.mojang.serialization.MapCodec) MAP_CODEC;
    }
    public ItemStack run(ItemStack itemStack, LootContext context) {
        CustomData.update(DataComponents.CUSTOM_DATA, itemStack, existing -> existing.merge(tag));
        return itemStack;
    }
    public static LootItemConditionalFunction.Builder<?> setCustomData(CompoundTag value) {
        return simpleBuilder(conditions -> new SetCustomDataFunction(conditions, value));
    }
}
