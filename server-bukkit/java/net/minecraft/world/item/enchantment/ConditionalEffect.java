// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.item.enchantment;
import com.mojang.serialization.Codec;
import java.util.Optional;
import net.minecraft.core.Holder;
import net.minecraft.world.level.storage.loot.LootContext;
import net.minecraft.world.level.storage.loot.Validatable;
import net.minecraft.world.level.storage.loot.ValidationContext;
import net.minecraft.world.level.storage.loot.predicates.LootItemCondition;
public record ConditionalEffect<T>(T effect, Optional<Holder<LootItemCondition>> requirements) implements Validatable {
    public static <T> Codec<ConditionalEffect<T>> codec(Codec<T> effectCodec) {
        return com.mojang.serialization.codecs.RecordCodecBuilder.create(i -> i.group(
            effectCodec.fieldOf("effect").forGetter(ConditionalEffect::effect),
            LootItemCondition.CODEC.optionalFieldOf("requirements").forGetter(ConditionalEffect::requirements))
            .apply(i, ConditionalEffect::new));
    }
    public boolean matches(LootContext context) {
        return requirements.isEmpty() || requirements.get().value().test(context);
    }
    public void validate(ValidationContext context) {
        return;
    }
}
