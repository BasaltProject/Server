// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.storage.loot.functions;
import net.minecraft.core.component.DataComponents;
import net.minecraft.core.registries.Registries;
import net.minecraft.world.item.alchemy.PotionContents;
import com.mojang.serialization.MapCodec;
import java.util.Optional;
import net.minecraft.core.Holder;
import net.minecraft.core.HolderSet;
import net.minecraft.world.item.ItemStack;
import net.minecraft.world.item.alchemy.Potion;
import net.minecraft.world.level.storage.loot.LootContext;
import net.minecraft.world.level.storage.loot.predicates.LootItemCondition;
public class SetRandomPotionFunction extends LootItemConditionalFunction {
    private final Optional<HolderSet<Potion>> options;
    public static final MapCodec<SetRandomPotionFunction> MAP_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(net.minecraft.world.level.storage.loot.functions.SetRandomPotionFunction.class, "MAP_CODEC");
    private SetRandomPotionFunction(Optional<Holder<LootItemCondition>> condition, Optional<HolderSet<Potion>> options) {
        super(condition);
        this.options = options;
    }
    public MapCodec<SetRandomPotionFunction> codec() {
        return (com.mojang.serialization.MapCodec) MAP_CODEC;
    }
    public ItemStack run(ItemStack itemStack, LootContext context) {
        Optional<? extends Holder<Potion>> pick = options.isPresent() ? options.get().getRandomElement(context.getRandom())
            : context.getLevel().registryAccess().lookupOrThrow(Registries.POTION).getRandom(context.getRandom());
        pick.ifPresent(chosen -> itemStack.update(DataComponents.POTION_CONTENTS, PotionContents.EMPTY, chosen, PotionContents::withPotion));
        return itemStack;
    }
    public static LootItemConditionalFunction.Builder<?> fromTagKey(HolderSet<Potion> tagKey) {
        return simpleBuilder(conditions -> new SetRandomPotionFunction(conditions, Optional.of(tagKey)));
    }
}
