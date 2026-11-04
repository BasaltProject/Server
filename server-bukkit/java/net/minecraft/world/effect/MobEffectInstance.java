// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.effect;

import com.google.common.collect.ComparisonChain;
import com.mojang.serialization.Codec;
import com.mojang.serialization.MapCodec;
import com.mojang.serialization.codecs.RecordCodecBuilder;
import io.netty.buffer.ByteBuf;
import it.unimi.dsi.fastutil.ints.Int2IntFunction;
import java.util.Optional;
import net.minecraft.core.Holder;
import net.minecraft.core.particles.ParticleOptions;
import net.minecraft.network.RegistryFriendlyByteBuf;
import net.minecraft.network.codec.ByteBufCodecs;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.util.Mth;
import net.minecraft.world.damagesource.DamageSource;
import net.minecraft.world.entity.Entity;
import net.minecraft.world.entity.LivingEntity;

public class MobEffectInstance implements Comparable<MobEffectInstance> {
    public static final int INFINITE_DURATION = -1;
    public static final int MIN_AMPLIFIER = 0;
    public static final int MAX_AMPLIFIER = 255;
    public static final Codec<MobEffectInstance> CODEC = Codec.lazyInitialized(() -> RecordCodecBuilder.create(instance -> instance.group(
        MobEffect.CODEC.fieldOf("id").forGetter(MobEffectInstance::getEffect),
        Details.MAP_CODEC.forGetter(MobEffectInstance::asDetails)).apply(instance, MobEffectInstance::new)));
    public static final StreamCodec<RegistryFriendlyByteBuf, MobEffectInstance> STREAM_CODEC = StreamCodec.of(
        (buf, value) -> {
            MobEffect.STREAM_CODEC.encode(buf, value.effect);
            Details.STREAM_CODEC.encode(buf, value.asDetails());
        },
        buf -> new MobEffectInstance(MobEffect.STREAM_CODEC.decode(buf), Details.STREAM_CODEC.decode(buf)));

    private final Holder<MobEffect> effect;
    private int duration;
    private int amplifier;
    private boolean ambient;
    private boolean visible;
    private boolean showIcon;
    private MobEffectInstance hiddenEffect;
    private final BlendState blendState = new BlendState();

    public MobEffectInstance(Holder<MobEffect> effect) {
        this(effect, 0, 0);
    }

    public MobEffectInstance(Holder<MobEffect> effect, int duration) {
        this(effect, duration, 0);
    }

    public MobEffectInstance(Holder<MobEffect> effect, int duration, int amplifier) {
        this(effect, duration, amplifier, false, true);
    }

    public MobEffectInstance(Holder<MobEffect> effect, int duration, int amplifier, boolean ambient, boolean visible) {
        this(effect, duration, amplifier, ambient, visible, visible);
    }

    public MobEffectInstance(Holder<MobEffect> effect, int duration, int amplifier, boolean ambient, boolean visible, boolean showIcon) {
        this(effect, duration, amplifier, ambient, visible, showIcon, null);
    }

    public MobEffectInstance(Holder<MobEffect> effect, int duration, int amplifier, boolean ambient, boolean visible, boolean showIcon,
        MobEffectInstance hiddenEffect) {
        this.effect = effect;
        this.duration = duration;
        this.amplifier = Mth.clamp(amplifier, MIN_AMPLIFIER, MAX_AMPLIFIER);
        this.ambient = ambient;
        this.visible = visible;
        this.showIcon = showIcon;
        this.hiddenEffect = hiddenEffect;
    }

    public MobEffectInstance(MobEffectInstance copy) {
        this.effect = copy.effect;
        copyDetails(copy);
    }

    private MobEffectInstance(Holder<MobEffect> effect, Details details) {
        this(effect, details.duration(), details.amplifier(), details.ambient(), details.showParticles(), details.showIcon(),
            details.hiddenEffect().map(hidden -> new MobEffectInstance(effect, hidden)).orElse(null));
    }

    private Details asDetails() {
        return new Details(amplifier, duration, ambient, visible, showIcon, Optional.ofNullable(hiddenEffect).map(MobEffectInstance::asDetails));
    }

    private void copyDetails(MobEffectInstance copy) {
        duration = copy.duration;
        amplifier = copy.amplifier;
        ambient = copy.ambient;
        visible = copy.visible;
        showIcon = copy.showIcon;
    }

    public float getBlendFactor(LivingEntity livingEntity, float partialTickTime) {
        return blendState.getFactor(livingEntity, partialTickTime);
    }

    public ParticleOptions getParticleOptions() {
        return effect.value().createParticleOptions(this);
    }

    public boolean update(MobEffectInstance takeOver) {
        boolean changed = false;
        if (takeOver.amplifier > amplifier) {
            if (takeOver.isShorterDurationThan(this)) {
                MobEffectInstance previousHidden = hiddenEffect;
                hiddenEffect = new MobEffectInstance(this);
                hiddenEffect.hiddenEffect = previousHidden;
            }
            amplifier = takeOver.amplifier;
            duration = takeOver.duration;
            changed = true;
        } else if (isShorterDurationThan(takeOver)) {
            if (takeOver.amplifier == amplifier) {
                duration = takeOver.duration;
                changed = true;
            } else if (hiddenEffect == null) {
                hiddenEffect = new MobEffectInstance(takeOver);
            } else {
                hiddenEffect.update(takeOver);
            }
        }
        if ((!takeOver.ambient && ambient) || changed) {
            ambient = takeOver.ambient;
            changed = true;
        }
        if (takeOver.visible != visible) {
            visible = takeOver.visible;
            changed = true;
        }
        if (takeOver.showIcon != showIcon) {
            showIcon = takeOver.showIcon;
            changed = true;
        }
        return changed;
    }

    private boolean isShorterDurationThan(MobEffectInstance other) {
        return !isInfiniteDuration() && (duration < other.duration || other.isInfiniteDuration());
    }

    public boolean isInfiniteDuration() {
        return duration == INFINITE_DURATION;
    }

    public boolean endsWithin(int ticks) {
        return !isInfiniteDuration() && duration <= ticks;
    }

    public MobEffectInstance withScaledDuration(float scale) {
        MobEffectInstance copy = new MobEffectInstance(this);
        copy.duration = copy.mapDuration(value -> Math.max(Mth.floor(value * scale), 1));
        return copy;
    }

    public int mapDuration(Int2IntFunction mapper) {
        return !isInfiniteDuration() && duration != 0 ? mapper.applyAsInt(duration) : duration;
    }

    public Holder<MobEffect> getEffect() {
        return effect;
    }

    public int getDuration() {
        return duration;
    }

    public int getAmplifier() {
        return amplifier;
    }

    public boolean isAmbient() {
        return ambient;
    }

    public boolean isVisible() {
        return visible;
    }

    public boolean showIcon() {
        return showIcon;
    }

    public boolean tickServer(ServerLevel serverLevel, LivingEntity target, Runnable onEffectUpdate) {
        if (!hasRemainingDuration()) {
            return false;
        }
        int tickCount = isInfiniteDuration() ? target.tickCount : duration;
        if (effect.value().shouldApplyEffectTickThisTick(tickCount, amplifier) && !effect.value().applyEffectTick(serverLevel, target, amplifier)) {
            return false;
        }
        tickDownDuration();
        if (downgradeToHiddenEffect()) {
            onEffectUpdate.run();
        }
        return hasRemainingDuration();
    }

    public void tickClient() {
        if (hasRemainingDuration()) {
            tickDownDuration();
            downgradeToHiddenEffect();
        }
        blendState.tick(this);
    }

    private boolean hasRemainingDuration() {
        return isInfiniteDuration() || duration > 0;
    }

    private void tickDownDuration() {
        if (hiddenEffect != null) {
            hiddenEffect.tickDownDuration();
        }
        duration = mapDuration(value -> value - 1);
    }

    private boolean downgradeToHiddenEffect() {
        if (duration == 0 && hiddenEffect != null) {
            copyDetails(hiddenEffect);
            hiddenEffect = hiddenEffect.hiddenEffect;
            return true;
        }
        return false;
    }

    public void onEffectStarted(LivingEntity mob) {
        effect.value().onEffectStarted(mob, amplifier);
    }

    public void onMobRemoved(ServerLevel level, LivingEntity mob, Entity.RemovalReason reason) {
        effect.value().onMobRemoved(level, mob, amplifier, reason);
    }

    public void onMobHurt(ServerLevel level, LivingEntity mob, DamageSource source, float damage) {
        effect.value().onMobHurt(level, mob, amplifier, source, damage);
    }

    public String getDescriptionId() {
        return effect.value().getDescriptionId();
    }

    @Override
    public String toString() {
        String result = getDescriptionId() + (amplifier > 0 ? " x " + (amplifier + 1) : "") + ", Duration: "
            + (isInfiniteDuration() ? "infinite" : Integer.toString(duration));
        if (!visible) {
            result += ", Particles: false";
        }
        if (!showIcon) {
            result += ", Show Icon: false";
        }
        return result;
    }

    @Override
    public boolean equals(Object o) {
        if (this == o) {
            return true;
        }
        return o instanceof MobEffectInstance that && duration == that.duration && amplifier == that.amplifier && ambient == that.ambient
            && visible == that.visible && showIcon == that.showIcon && effect.equals(that.effect);
    }

    @Override
    public int hashCode() {
        int result = effect.hashCode();
        result = 31 * result + duration;
        result = 31 * result + amplifier;
        result = 31 * result + (ambient ? 1 : 0);
        result = 31 * result + (visible ? 1 : 0);
        result = 31 * result + (showIcon ? 1 : 0);
        return result;
    }

    @Override
    public int compareTo(MobEffectInstance o) {
        int colorA = effect.value().getColor();
        int colorB = o.effect.value().getColor();
        if ((getDuration() <= 32147 || o.getDuration() <= 32147) && (!isAmbient() || !o.isAmbient())) {
            return ComparisonChain.start().compareFalseFirst(isAmbient(), o.isAmbient())
                .compareFalseFirst(isInfiniteDuration(), o.isInfiniteDuration()).compare(getDuration(), o.getDuration())
                .compare(colorA, colorB).result();
        }
        return ComparisonChain.start().compare(isAmbient(), o.isAmbient()).compare(colorA, colorB).result();
    }

    public void onEffectAdded(LivingEntity livingEntity) {
        effect.value().onEffectAdded(livingEntity, amplifier);
    }

    public boolean is(Holder<MobEffect> effect) {
        return this.effect.equals(effect);
    }

    public void copyBlendState(MobEffectInstance instance) {
        blendState.copyFrom(instance.blendState);
    }

    public void skipBlending() {
        blendState.setImmediate(this);
    }

    static record Details(int amplifier, int duration, boolean ambient, boolean showParticles, boolean showIcon, Optional<Details> hiddenEffect) {
        public static final MapCodec<Details> MAP_CODEC = MapCodec.recursive("MobEffectInstance.Details", codec -> RecordCodecBuilder.mapCodec(
            instance -> instance.group(Codec.intRange(0, 255).optionalFieldOf("amplifier", 0).forGetter(Details::amplifier),
                Codec.INT.optionalFieldOf("duration", 0).forGetter(Details::duration),
                Codec.BOOL.optionalFieldOf("ambient", false).forGetter(Details::ambient),
                Codec.BOOL.optionalFieldOf("show_particles", true).forGetter(Details::showParticles),
                Codec.BOOL.optionalFieldOf("show_icon").forGetter(details -> Optional.of(details.showIcon())),
                codec.optionalFieldOf("hidden_effect").forGetter(Details::hiddenEffect)).apply(instance, Details::create)));
        public static final StreamCodec<ByteBuf, Details> STREAM_CODEC = StreamCodec.of(Details::write, Details::read);

        private static void write(ByteBuf buf, Details details) {
            ByteBufCodecs.VAR_INT.encode(buf, details.amplifier);
            ByteBufCodecs.VAR_INT.encode(buf, details.duration);
            ByteBufCodecs.BOOL.encode(buf, details.ambient);
            ByteBufCodecs.BOOL.encode(buf, details.showParticles);
            ByteBufCodecs.BOOL.encode(buf, details.showIcon);
            ByteBufCodecs.BOOL.encode(buf, details.hiddenEffect.isPresent());
            details.hiddenEffect.ifPresent(hidden -> write(buf, hidden));
        }

        private static Details read(ByteBuf buf) {
            int amplifier = ByteBufCodecs.VAR_INT.decode(buf);
            int duration = ByteBufCodecs.VAR_INT.decode(buf);
            boolean ambient = ByteBufCodecs.BOOL.decode(buf);
            boolean particles = ByteBufCodecs.BOOL.decode(buf);
            boolean icon = ByteBufCodecs.BOOL.decode(buf);
            Optional<Details> hidden = ByteBufCodecs.BOOL.decode(buf) ? Optional.of(read(buf)) : Optional.empty();
            return new Details(amplifier, duration, ambient, particles, icon, hidden);
        }

        private static Details create(int amplifier, int duration, boolean ambient, boolean showParticles, Optional<Boolean> showIcon,
            Optional<Details> hiddenEffect) {
            return new Details(amplifier, duration, ambient, showParticles, showIcon.orElse(showParticles), hiddenEffect);
        }
    }

    static class BlendState {
        private float factor;
        private float factorPreviousFrame;

        public void setImmediate(MobEffectInstance instance) {
            factor = computeTarget(instance);
            factorPreviousFrame = factor;
        }

        public void copyFrom(BlendState other) {
            factor = other.factor;
            factorPreviousFrame = other.factorPreviousFrame;
        }

        public void tick(MobEffectInstance instance) {
            factorPreviousFrame = factor;
            float target = computeTarget(instance);
            int blendIn = instance.effect.value().getBlendInDurationTicks();
            float step = blendIn <= 0 ? 1.0F : 1.0F / blendIn;
            factor += Mth.clamp(target - factor, -step, step);
        }

        public float getFactor(LivingEntity livingEntity, float partialTickTime) {
            return Mth.lerp(partialTickTime, factorPreviousFrame, factor);
        }

        private static float computeTarget(MobEffectInstance instance) {
            boolean ending = !instance.isInfiniteDuration() && instance.getDuration() <= instance.effect.value().getBlendOutAdvanceTicks();
            return ending ? 0.0F : 1.0F;
        }
    }
}
