// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.item.enchantment.effects;
import net.minecraft.world.entity.LivingEntity;
import net.minecraft.world.entity.ai.attributes.AttributeInstance;
import com.mojang.serialization.MapCodec;
import net.minecraft.core.Holder;
import net.minecraft.resources.Identifier;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.util.StringRepresentable;
import net.minecraft.world.entity.Entity;
import net.minecraft.world.entity.ai.attributes.Attribute;
import net.minecraft.world.entity.ai.attributes.AttributeModifier;
import net.minecraft.world.item.enchantment.EnchantedItemInUse;
import net.minecraft.world.item.enchantment.LevelBasedValue;
import net.minecraft.world.phys.Vec3;
public record EnchantmentAttributeEffect(Identifier id, Holder<Attribute> attribute, LevelBasedValue amount, AttributeModifier.Operation operation) implements EnchantmentLocationBasedEffect {
    public static final MapCodec<EnchantmentAttributeEffect> MAP_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(net.minecraft.world.item.enchantment.effects.EnchantmentAttributeEffect.class, "MAP_CODEC");
    public AttributeModifier getModifier(int level, StringRepresentable slot) {
        return new AttributeModifier(id.withSuffix("/" + slot.getSerializedName()), amount.calculate(level), operation);
    }
    public void onChangedBlock(ServerLevel serverLevel, int enchantmentLevel, EnchantedItemInUse item, Entity entity, Vec3 position, boolean becameActive) {
        if (becameActive && entity instanceof LivingEntity living) {
            AttributeInstance target = living.getAttribute(attribute);
            if (target != null) {
                target.addOrReplacePermanentModifier(getModifier(enchantmentLevel, item.inSlot()));
            }
        }
    }
    public void onDeactivated(EnchantedItemInUse item, Entity entity, Vec3 position, int level) {
        if (entity instanceof LivingEntity living) {
            AttributeInstance target = living.getAttribute(attribute);
            if (target != null) {
                target.removeModifier(getModifier(level, item.inSlot()).id());
            }
        }


    }
    public MapCodec<EnchantmentAttributeEffect> codec() {
        return (com.mojang.serialization.MapCodec) MAP_CODEC;
    }
}
