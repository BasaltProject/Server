// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.effect;

import com.mojang.serialization.Codec;
import java.util.LinkedHashMap;
import java.util.Map;
import java.util.function.BiConsumer;
import net.minecraft.core.Holder;
import net.minecraft.core.particles.ParticleOptions;
import net.minecraft.network.RegistryFriendlyByteBuf;
import net.minecraft.network.chat.Component;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.resources.Identifier;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.sounds.SoundEvent;
import net.minecraft.world.damagesource.DamageSource;
import net.minecraft.world.entity.Entity;
import net.minecraft.world.entity.LivingEntity;
import net.minecraft.world.entity.ai.attributes.Attribute;
import net.minecraft.world.entity.ai.attributes.AttributeInstance;
import net.minecraft.world.entity.ai.attributes.AttributeMap;
import net.minecraft.world.entity.ai.attributes.AttributeModifier;
import net.minecraft.world.flag.FeatureElement;
import net.minecraft.world.flag.FeatureFlag;
import net.minecraft.world.flag.FeatureFlagSet;

public class MobEffect implements FeatureElement {
    public static final Codec<Holder<MobEffect>> CODEC = Codec.STRING.xmap(
        name -> MobEffects.registry().get(Identifier.parse(name)).orElseThrow(), holder -> holder.unwrapKey().orElseThrow().identifier().toString());
    public static final StreamCodec<RegistryFriendlyByteBuf, Holder<MobEffect>> STREAM_CODEC = StreamCodec.of(
        (buf, holder) -> net.minecraft.network.codec.ByteBufCodecs.VAR_INT.encode(buf, MobEffects.registry().getId(holder.value())),
        buf -> MobEffects.registry().get(net.minecraft.network.codec.ByteBufCodecs.VAR_INT.decode(buf)).orElseThrow());

    private final Map<Holder<Attribute>, AttributeTemplate> attributeModifiers = new LinkedHashMap<>();
    private final MobEffectCategory category;
    private final int color;
    private final ParticleOptions fixedParticle;
    private String descriptionId;
    private int blendInDurationTicks;
    private int blendOutDurationTicks;
    private int blendOutAdvanceTicks;
    private FeatureFlagSet requiredFeatures;

    protected MobEffect(MobEffectCategory category, int color) {
        this(category, color, null);
    }

    protected MobEffect(MobEffectCategory category, int color, ParticleOptions particleOptions) {
        this.category = category;
        this.color = color;
        this.fixedParticle = particleOptions;
    }

    public int getBlendInDurationTicks() {
        return blendInDurationTicks;
    }

    public int getBlendOutDurationTicks() {
        return blendOutDurationTicks;
    }

    public int getBlendOutAdvanceTicks() {
        return blendOutAdvanceTicks;
    }

    public boolean applyEffectTick(ServerLevel serverLevel, LivingEntity mob, int amplification) {
        return true;
    }

    public void applyInstantaneousEffect(ServerLevel level, Entity source, Entity owner, LivingEntity mob, int amplification, double scale) {
        applyEffectTick(level, mob, amplification);
    }

    public boolean shouldApplyEffectTickThisTick(int tickCount, int amplification) {
        return false;
    }

    public void onEffectStarted(LivingEntity mob, int amplifier) {
    }

    public void onEffectAdded(LivingEntity mob, int amplifier) {
    }

    public void onMobRemoved(ServerLevel level, LivingEntity mob, int amplifier, Entity.RemovalReason reason) {
    }

    public void onMobHurt(ServerLevel level, LivingEntity mob, int amplifier, DamageSource source, float damage) {
    }

    public boolean isInstantaneous() {
        return false;
    }

    protected String getOrCreateDescriptionId() {
        if (descriptionId == null) {
            Identifier key = MobEffects.registry().getKey(this);
            descriptionId = "effect." + key.getNamespace() + "." + key.getPath().replace('/', '.');
        }
        return descriptionId;
    }

    public String getDescriptionId() {
        return getOrCreateDescriptionId();
    }

    public Component getDisplayName() {
        return Component.translatable(getDescriptionId());
    }

    public MobEffectCategory getCategory() {
        return category;
    }

    public int getColor() {
        return color;
    }

    public MobEffect addAttributeModifier(Holder<Attribute> attribute, Identifier id, double amount, AttributeModifier.Operation operation) {
        attributeModifiers.put(attribute, new AttributeTemplate(id, amount, operation));
        return this;
    }

    public MobEffect setBlendDuration(int ticks) {
        return setBlendDuration(ticks, ticks, ticks);
    }

    public MobEffect setBlendDuration(int inTicks, int outTicks, int outAdvanceTicks) {
        blendInDurationTicks = inTicks;
        blendOutDurationTicks = outTicks;
        blendOutAdvanceTicks = outAdvanceTicks;
        return this;
    }

    public void createModifiers(int amplifier, BiConsumer<Holder<Attribute>, AttributeModifier> consumer) {
        attributeModifiers.forEach((attribute, template) -> consumer.accept(attribute, template.create(amplifier)));
    }

    public void removeAttributeModifiers(AttributeMap attributes) {
        attributeModifiers.forEach((attribute, template) -> {
            AttributeInstance instance = attributes.getInstance(attribute);
            if (instance != null) {
                instance.removeModifier(template.id());
            }
        });
    }

    public void addAttributeModifiers(AttributeMap attributes, int amplifier) {
        attributeModifiers.forEach((attribute, template) -> {
            AttributeInstance instance = attributes.getInstance(attribute);
            if (instance != null) {
                instance.removeModifier(template.id());
                instance.addPermanentModifier(template.create(amplifier));
            }
        });
    }

    public boolean isBeneficial() {
        return category == MobEffectCategory.BENEFICIAL;
    }

    public ParticleOptions createParticleOptions(MobEffectInstance mobEffectInstance) {
        return fixedParticle;
    }

    public MobEffect withSoundOnAdded(SoundEvent soundEvent) {
        return this;
    }

    public MobEffect requiredFeatures(FeatureFlag... flags) {
        return this;
    }

    public FeatureFlagSet requiredFeatures() {
        return requiredFeatures;
    }

    static record AttributeTemplate(Identifier id, double amount, AttributeModifier.Operation operation) {
        public AttributeModifier create(int amplifier) {
            return new AttributeModifier(id, amount * (amplifier + 1), operation);
        }
    }
}
