// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.item.equipment;
import java.util.Map;
import net.minecraft.core.Holder;
import net.minecraft.resources.ResourceKey;
import net.minecraft.sounds.SoundEvent;
import net.minecraft.tags.TagKey;
import net.minecraft.world.item.Item;
import net.minecraft.world.item.component.ItemAttributeModifiers;
public record ArmorMaterial(int durability, Map<ArmorType, Integer> defense, int enchantmentValue, Holder<SoundEvent> equipSound, float toughness, float knockbackResistance, TagKey<Item> repairIngredient, ResourceKey<EquipmentAsset> assetId) {
    public ItemAttributeModifiers createAttributes(ArmorType type) {
        int armor = defense.getOrDefault(type, 0);
        net.minecraft.world.item.component.ItemAttributeModifiers.Builder modifiers = net.minecraft.world.item.component.ItemAttributeModifiers.builder();
        net.minecraft.world.entity.EquipmentSlotGroup group = net.minecraft.world.entity.EquipmentSlotGroup.bySlot(type.getSlot());
        net.minecraft.resources.Identifier id = net.minecraft.resources.Identifier.withDefaultNamespace("armor." + type.getName());
        var add = net.minecraft.world.entity.ai.attributes.AttributeModifier.Operation.ADD_VALUE;
        modifiers.add(net.minecraft.world.entity.ai.attributes.Attributes.ARMOR, new net.minecraft.world.entity.ai.attributes.AttributeModifier(id, armor, add), group);
        modifiers.add(net.minecraft.world.entity.ai.attributes.Attributes.ARMOR_TOUGHNESS, new net.minecraft.world.entity.ai.attributes.AttributeModifier(id, toughness, add), group);
        if (knockbackResistance > 0.0F) {
            modifiers.add(net.minecraft.world.entity.ai.attributes.Attributes.KNOCKBACK_RESISTANCE, new net.minecraft.world.entity.ai.attributes.AttributeModifier(id, knockbackResistance, add), group);
        }
        return modifiers.build();
    }
}
