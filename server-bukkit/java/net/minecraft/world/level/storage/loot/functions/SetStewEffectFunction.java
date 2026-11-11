// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.storage.loot.functions;
import net.minecraft.core.component.DataComponents;
import net.minecraft.world.item.Items;
import net.minecraft.world.item.component.SuspiciousStewEffects;
import net.minecraft.world.level.storage.loot.providers.number.ints.ContextIntProviders;
import com.mojang.serialization.Codec;
import com.mojang.serialization.MapCodec;
import java.util.List;
import java.util.Optional;
import net.minecraft.core.Holder;
import net.minecraft.world.effect.MobEffect;
import net.minecraft.world.item.ItemStack;
import net.minecraft.world.level.storage.loot.LootContext;
import net.minecraft.world.level.storage.loot.LootContextUser;
import net.minecraft.world.level.storage.loot.ValidationContext;
import net.minecraft.world.level.storage.loot.predicates.LootItemCondition;
import net.minecraft.world.level.storage.loot.providers.number.ints.ContextIntProvider;
public class SetStewEffectFunction extends LootItemConditionalFunction {
    private final List<EffectEntry> effects;
    public static final MapCodec<SetStewEffectFunction> MAP_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(net.minecraft.world.level.storage.loot.functions.SetStewEffectFunction.class, "MAP_CODEC");
    private SetStewEffectFunction(Optional<Holder<LootItemCondition>> condition, List<EffectEntry> effects) {
        super(condition);
        this.effects = effects;
    }
    public MapCodec<SetStewEffectFunction> codec() {
        return (com.mojang.serialization.MapCodec) MAP_CODEC;
    }
    public void validate(ValidationContext context) {
    }
    public ItemStack run(ItemStack itemStack, LootContext context) {
        if (itemStack.is(Items.SUSPICIOUS_STEW) && !effects.isEmpty()) {
            EffectEntry pick = effects.get(context.getRandom().nextInt(effects.size()));
            int ticks = pick.duration().value().getInt(context);
            if (!pick.effect().value().isInstantaneous()) {
                ticks *= 20;
            }
            SuspiciousStewEffects.Entry added = new SuspiciousStewEffects.Entry(pick.effect(), ticks);
            itemStack.update(DataComponents.SUSPICIOUS_STEW_EFFECTS, SuspiciousStewEffects.EMPTY, added, SuspiciousStewEffects::withEffectAdded);
        }
        return itemStack;
    }
    public static Builder stewEffect() {
        return new Builder();
    }
    public static class Builder extends LootItemConditionalFunction.Builder<Builder> {
        private final List<EffectEntry> entries = new java.util.ArrayList<>();
        protected Builder getThis() {
            return this;
        }
        public Builder withEffect(Holder<MobEffect> effect, Holder<ContextIntProvider> duration) {
            entries.add(new EffectEntry(effect, duration));
            return this;
        }
        public Builder withEffect(Holder<MobEffect> effect, int duration) {
            return withEffect(effect, ContextIntProviders.exactly(duration));
        }
        public LootItemFunction build() {
            return new SetStewEffectFunction(getCondition(), List.copyOf(entries));
        }
    }
    static record EffectEntry(Holder<MobEffect> effect, Holder<ContextIntProvider> duration) implements LootContextUser {
        public static final Codec<EffectEntry> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.codec(net.minecraft.world.level.storage.loot.functions.SetStewEffectFunction.EffectEntry.class, "CODEC");
        public void validate(ValidationContext context) {
        }
    }
}
