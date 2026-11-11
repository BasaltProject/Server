// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity;
import net.minecraft.resources.Identifier;
import com.mojang.serialization.Codec;
import java.util.List;
import java.util.Optional;
import net.minecraft.core.Holder;
import net.minecraft.core.HolderSet;
import net.minecraft.resources.ResourceKey;
import net.minecraft.sounds.SoundEvent;
import net.minecraft.util.valueproviders.FloatProvider;
import net.minecraft.world.damagesource.DamageType;
import net.minecraft.world.entity.ai.attributes.Attribute;
import net.minecraft.world.entity.ai.attributes.AttributeModifier;
import net.minecraft.world.item.Item;
public record SulfurCubeArchetype(HolderSet<Item> items, List<AttributeEntry> attributeModifiers, boolean buoyant, Optional<ExplosionData> explosion, Optional<ContactDamage> contactDamage, KnockbackModifiers knockbackModifiers, SoundSettings soundSettings) {
    public static final Codec<SulfurCubeArchetype> DIRECT_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.codec(net.minecraft.world.entity.SulfurCubeArchetype.class, "DIRECT_CODEC");
    public static KnockbackModifiers DEFAULT_KNOCKBACK_MODIFIERS = new KnockbackModifiers(0.33F, 0.06F);
    public static SoundSettings DEFAULT_SOUND_SETTINGS = new SoundSettings(net.minecraft.sounds.SoundEvents.SULFUR_CUBE_REGULAR_HIT, net.minecraft.sounds.SoundEvents.SULFUR_CUBE_REGULAR_PUSH, 0.2F, 0.5F);
    public static record AttributeEntry(Holder<Attribute> attribute, AttributeModifier modifier) {
        public static final Codec<AttributeEntry> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.codec(net.minecraft.world.entity.SulfurCubeArchetype.AttributeEntry.class, "CODEC");
        public static AttributeEntry add(Holder<Attribute> attribute, double amount, ResourceKey<SulfurCubeArchetype> archetype) {
            String stem = archetype.identifier().getPath();
            return new AttributeEntry(attribute, new AttributeModifier(Identifier.withDefaultNamespace(stem + "_add_" + attribute.unwrapKey().get().identifier().getPath()), amount, AttributeModifier.Operation.ADD_VALUE));
        }
        public static AttributeEntry multiply(Holder<Attribute> attribute, double amount, ResourceKey<SulfurCubeArchetype> archetype) {
            String stem = archetype.identifier().getPath();
            return new AttributeEntry(attribute, new AttributeModifier(Identifier.withDefaultNamespace(stem + "_mul_" + attribute.unwrapKey().get().identifier().getPath()), amount - 1.0, AttributeModifier.Operation.ADD_MULTIPLIED_TOTAL));


        }
    }
    public static record ExplosionData(int power, boolean causesFire, int fuse) {
        public static final Codec<ExplosionData> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.codec(net.minecraft.world.entity.SulfurCubeArchetype.ExplosionData.class, "CODEC");
    }
    public static record ContactDamage(Holder<DamageType> damageType, FloatProvider amount, boolean attributeToSource) {
        public static final Codec<ContactDamage> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.codec(net.minecraft.world.entity.SulfurCubeArchetype.ContactDamage.class, "CODEC");
    }
    public static record KnockbackModifiers(float horizontalPower, float verticalPower) {
        public static final Codec<KnockbackModifiers> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.codec(net.minecraft.world.entity.SulfurCubeArchetype.KnockbackModifiers.class, "CODEC");
    }
    public static record SoundSettings(Holder<SoundEvent> hitSound, Holder<SoundEvent> pushSound, float pushSoundImpulseThreshold, float pushSoundCooldown) {
        public static final Codec<SoundSettings> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.codec(net.minecraft.world.entity.SulfurCubeArchetype.SoundSettings.class, "CODEC");
    }
}
