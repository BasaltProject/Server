// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.advancements.predicates;
import com.mojang.serialization.Codec;
import com.mojang.serialization.codecs.RecordCodecBuilder;
import java.util.HashMap;
import java.util.LinkedHashMap;
import java.util.Map;
import java.util.Optional;
import java.util.function.Predicate;
import net.minecraft.core.Holder;
import net.minecraft.network.RegistryFriendlyByteBuf;
import net.minecraft.network.codec.ByteBufCodecs;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.world.effect.MobEffect;
import net.minecraft.world.effect.MobEffectInstance;
import net.minecraft.world.entity.Entity;
import net.minecraft.world.entity.LivingEntity;
public record MobEffectsPredicate(Map<Holder<MobEffect>, MobEffectInstancePredicate> effectMap) implements Predicate<MobEffectInstance> {
    public static final Codec<MobEffectsPredicate> CODEC = Codec.unboundedMap(MobEffect.CODEC, MobEffectInstancePredicate.CODEC).xmap(MobEffectsPredicate::new, MobEffectsPredicate::effectMap);
    public static final StreamCodec<RegistryFriendlyByteBuf, MobEffectsPredicate> STREAM_CODEC = MobEffectInstancePredicate.MAP_STREAM_CODEC.map(MobEffectsPredicate::new, MobEffectsPredicate::effectMap);
    public boolean matches(Entity entity) {
        return entity instanceof LivingEntity living && matches(living.getActiveEffectsMap());
    }
    public boolean matches(LivingEntity entity) {
        return matches(entity.getActiveEffectsMap());
    }
    public boolean matches(Map<Holder<MobEffect>, MobEffectInstance> effects) {
        for (Map.Entry<Holder<MobEffect>, MobEffectInstancePredicate> entry : effectMap.entrySet()) {
            if (!entry.getValue().matches(effects.get(entry.getKey()))) {
                return false;
            }
        }
        return true;
    }
    public boolean test(MobEffectInstance effect) {
        MobEffectInstancePredicate predicate = effectMap.get(effect.getEffect());
        return predicate != null && predicate.matches(effect);
    }
    public static class Builder {
        private final Map<Holder<MobEffect>, MobEffectInstancePredicate> effects = new LinkedHashMap<>();
        public static Builder effects() {
            return new Builder();
        }
        public Builder and(Holder<MobEffect> effect) {
            effects.put(effect, new MobEffectInstancePredicate());
            return this;
        }
        public Builder and(Holder<MobEffect> effect, MobEffectInstancePredicate predicate) {
            effects.put(effect, predicate);
            return this;
        }
        public MobEffectsPredicate build() {
            return new MobEffectsPredicate(Map.copyOf(effects));
        }
    }
    public static record MobEffectInstancePredicate(MinMaxBounds.Ints amplifier, MinMaxBounds.Ints duration, Optional<Boolean> ambient, Optional<Boolean> visible) {
        public static final Codec<MobEffectInstancePredicate> CODEC = RecordCodecBuilder.create(i -> i.group(
            MinMaxBounds.Ints.CODEC.optionalFieldOf("amplifier", MinMaxBounds.Ints.ANY).forGetter(MobEffectInstancePredicate::amplifier),
            MinMaxBounds.Ints.CODEC.optionalFieldOf("duration", MinMaxBounds.Ints.ANY).forGetter(MobEffectInstancePredicate::duration),
            Codec.BOOL.optionalFieldOf("ambient").forGetter(MobEffectInstancePredicate::ambient),
            Codec.BOOL.optionalFieldOf("visible").forGetter(MobEffectInstancePredicate::visible)).apply(i, MobEffectInstancePredicate::new));
        public static final StreamCodec<RegistryFriendlyByteBuf, MobEffectInstancePredicate> STREAM_CODEC = StreamCodec.composite(
            MinMaxBounds.Ints.STREAM_CODEC, MobEffectInstancePredicate::amplifier, MinMaxBounds.Ints.STREAM_CODEC, MobEffectInstancePredicate::duration,
            ByteBufCodecs.optional(ByteBufCodecs.BOOL), MobEffectInstancePredicate::ambient, ByteBufCodecs.optional(ByteBufCodecs.BOOL), MobEffectInstancePredicate::visible,
            MobEffectInstancePredicate::new);
        public static final StreamCodec<RegistryFriendlyByteBuf, Map<Holder<MobEffect>, MobEffectInstancePredicate>> MAP_STREAM_CODEC =
            ByteBufCodecs.map(HashMap::new, MobEffect.STREAM_CODEC, STREAM_CODEC);
        public MobEffectInstancePredicate() {
            this(MinMaxBounds.Ints.ANY, MinMaxBounds.Ints.ANY, Optional.empty(), Optional.empty());
        }
        public boolean matches(MobEffectInstance instance) {
            if (instance == null || !amplifier.matches(instance.getAmplifier()) || !duration.matches(instance.getDuration())) {
                return false;
            }
            if (ambient.isPresent() && ambient.get() != instance.isAmbient()) {
                return false;
            }
            return visible.isEmpty() || visible.get() == instance.isVisible();
        }
    }
}
