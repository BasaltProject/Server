// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.item.enchantment;
import com.mojang.serialization.Codec;
import java.util.Optional;
import net.minecraft.core.Holder;
import net.minecraft.world.level.storage.loot.LootContext;
import net.minecraft.world.level.storage.loot.Validatable;
import net.minecraft.world.level.storage.loot.ValidationContext;
import net.minecraft.world.level.storage.loot.predicates.LootItemCondition;
public record TargetedConditionalEffect<T>(EnchantmentTarget enchanted, EnchantmentTarget affected, T effect, Optional<Holder<LootItemCondition>> requirements) implements Validatable {
    public static <S> Codec<TargetedConditionalEffect<S>> codec(Codec<S> effectCodec) {
        return com.mojang.serialization.codecs.RecordCodecBuilder.create(i -> i.group(
            EnchantmentTarget.CODEC.fieldOf("enchanted").forGetter(TargetedConditionalEffect::enchanted),
            EnchantmentTarget.CODEC.fieldOf("affected").forGetter(TargetedConditionalEffect::affected),
            effectCodec.fieldOf("effect").forGetter(TargetedConditionalEffect::effect),
            LootItemCondition.CODEC.optionalFieldOf("requirements").forGetter(TargetedConditionalEffect::requirements))
            .apply(i, TargetedConditionalEffect::new));
    }
    public static <S> Codec<TargetedConditionalEffect<S>> equipmentDropsCodec(Codec<S> effectCodec) {
        return com.mojang.serialization.codecs.RecordCodecBuilder.create(i -> i.group(
            EnchantmentTarget.NON_DAMAGE_CODEC.fieldOf("enchanted").forGetter(TargetedConditionalEffect::enchanted),
            effectCodec.fieldOf("effect").forGetter(TargetedConditionalEffect::effect),
            LootItemCondition.CODEC.optionalFieldOf("requirements").forGetter(TargetedConditionalEffect::requirements))
            .apply(i, (target, effect, requirements) -> new TargetedConditionalEffect<>(target, EnchantmentTarget.VICTIM, effect, requirements)));
    }
    public boolean matches(LootContext context) {
        return requirements.isEmpty() || requirements.get().value().test(context);
    }
    public void validate(ValidationContext context) {
        return;
    }
}
