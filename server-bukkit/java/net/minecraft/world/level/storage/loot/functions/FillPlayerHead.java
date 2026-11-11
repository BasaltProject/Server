// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.storage.loot.functions;
import net.minecraft.core.component.DataComponents;
import net.minecraft.world.item.Items;
import net.minecraft.world.entity.player.Player;
import net.minecraft.world.item.component.ResolvableProfile;
import com.mojang.serialization.MapCodec;
import java.util.Optional;
import java.util.Set;
import net.minecraft.core.Holder;
import net.minecraft.util.context.ContextKey;
import net.minecraft.world.item.ItemStack;
import net.minecraft.world.level.storage.loot.LootContext;
import net.minecraft.world.level.storage.loot.predicates.LootItemCondition;
public class FillPlayerHead extends LootItemConditionalFunction {
    private final LootContext.EntityTarget entityTarget;
    public static final MapCodec<FillPlayerHead> MAP_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(net.minecraft.world.level.storage.loot.functions.FillPlayerHead.class, "MAP_CODEC");
    public FillPlayerHead(Optional<Holder<LootItemCondition>> condition, LootContext.EntityTarget entityTarget) {
        super(condition);
        this.entityTarget = entityTarget;
    }
    public MapCodec<FillPlayerHead> codec() {
        return (com.mojang.serialization.MapCodec) MAP_CODEC;
    }
    public Set<ContextKey<?>> getReferencedContextParams() {
        return java.util.Set.of(entityTarget.contextParam());
    }
    public ItemStack run(ItemStack itemStack, LootContext context) {
        if (itemStack.is(Items.PLAYER_HEAD) && context.getOptional(entityTarget.contextParam()) instanceof Player donor) {
            itemStack.set(DataComponents.PROFILE, ResolvableProfile.createResolved(donor.getGameProfile()));
        }
        return itemStack;
    }
    public static LootItemConditionalFunction.Builder<?> fillPlayerHead(LootContext.EntityTarget entityTarget) {
        return simpleBuilder(conditions -> new FillPlayerHead(conditions, entityTarget));


    }
}
