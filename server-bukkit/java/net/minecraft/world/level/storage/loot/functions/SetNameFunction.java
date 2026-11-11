// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.storage.loot.functions;
import net.minecraft.core.component.DataComponents;
import com.mojang.serialization.Codec;
import com.mojang.serialization.MapCodec;
import java.util.Optional;
import java.util.Set;
import java.util.function.UnaryOperator;
import net.minecraft.core.Holder;
import net.minecraft.core.component.DataComponentType;
import net.minecraft.network.chat.Component;
import net.minecraft.util.StringRepresentable;
import net.minecraft.util.context.ContextKey;
import net.minecraft.world.item.ItemStack;
import net.minecraft.world.level.storage.loot.LootContext;
import net.minecraft.world.level.storage.loot.predicates.LootItemCondition;
public class SetNameFunction extends LootItemConditionalFunction {
    private final Optional<Component> name;
    private final Optional<LootContext.EntityTarget> resolutionContext;
    private final Target target;
    public static final MapCodec<SetNameFunction> MAP_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(net.minecraft.world.level.storage.loot.functions.SetNameFunction.class, "MAP_CODEC");
    private SetNameFunction(Optional<Holder<LootItemCondition>> condition, Optional<Component> name, Optional<LootContext.EntityTarget> resolutionContext, Target target) {
        super(condition);
        this.name = name;
        this.resolutionContext = resolutionContext;
        this.target = target;
    }
    public MapCodec<SetNameFunction> codec() {
        return (com.mojang.serialization.MapCodec) MAP_CODEC;
    }
    public Set<ContextKey<?>> getReferencedContextParams() {
        return Set.of();
    }
    public static UnaryOperator<Component> createResolver(LootContext context, LootContext. EntityTarget entityTarget) {
        return line -> line;
    }
    public ItemStack run(ItemStack itemStack, LootContext context) {
        name.ifPresent(text -> itemStack.set(target.component(), text));
        return itemStack;
    }
    public static LootItemConditionalFunction.Builder<?> setName(Component value, Target target) {
        return simpleBuilder(c -> new SetNameFunction(c, Optional.of(value), Optional.empty(), target));
    }
    public static LootItemConditionalFunction.Builder<?> setName(Component value, Target target, LootContext.EntityTarget resolutionContext) {
        return simpleBuilder(c -> new SetNameFunction(c, Optional.of(value), Optional.of(resolutionContext), target));
    }
    public static enum Target implements StringRepresentable {
        CUSTOM_NAME, ITEM_NAME;
        public static final Codec<Target> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.codec(net.minecraft.world.level.storage.loot.functions.SetNameFunction.Target.class, "CODEC");
        public String getSerializedName() {
            return name().toLowerCase(java.util.Locale.ROOT);
        }
        public DataComponentType<Component> component() {
            return this == CUSTOM_NAME ? DataComponents.CUSTOM_NAME : DataComponents.ITEM_NAME;
        }
    }
}
