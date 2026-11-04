// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.item.equipment;
import java.util.Map;
public interface ArmorMaterials {
    ArmorMaterial LEATHER = new ArmorMaterial(5, defense(1, 2, 3, 1, 3), 15, net.minecraft.sounds.SoundEvents.ARMOR_EQUIP_LEATHER, 0F, 0F, net.minecraft.tags.ItemTags.REPAIRS_LEATHER_ARMOR, EquipmentAssets.LEATHER);
    ArmorMaterial COPPER = new ArmorMaterial(11, defense(1, 3, 4, 2, 4), 8, net.minecraft.sounds.SoundEvents.ARMOR_EQUIP_COPPER, 0F, 0F, net.minecraft.tags.ItemTags.REPAIRS_COPPER_ARMOR, EquipmentAssets.COPPER);
    ArmorMaterial CHAINMAIL = new ArmorMaterial(15, defense(1, 4, 5, 2, 4), 12, net.minecraft.sounds.SoundEvents.ARMOR_EQUIP_CHAIN, 0F, 0F, net.minecraft.tags.ItemTags.REPAIRS_CHAIN_ARMOR, EquipmentAssets.CHAINMAIL);
    ArmorMaterial IRON = new ArmorMaterial(15, defense(2, 5, 6, 2, 5), 9, net.minecraft.sounds.SoundEvents.ARMOR_EQUIP_IRON, 0F, 0F, net.minecraft.tags.ItemTags.REPAIRS_IRON_ARMOR, EquipmentAssets.IRON);
    ArmorMaterial GOLD = new ArmorMaterial(7, defense(1, 3, 5, 2, 7), 25, net.minecraft.sounds.SoundEvents.ARMOR_EQUIP_GOLD, 0F, 0F, net.minecraft.tags.ItemTags.REPAIRS_GOLD_ARMOR, EquipmentAssets.GOLD);
    ArmorMaterial DIAMOND = new ArmorMaterial(33, defense(3, 6, 8, 3, 11), 10, net.minecraft.sounds.SoundEvents.ARMOR_EQUIP_DIAMOND, 2F, 0F, net.minecraft.tags.ItemTags.REPAIRS_DIAMOND_ARMOR, EquipmentAssets.DIAMOND);
    ArmorMaterial TURTLE_SCUTE = new ArmorMaterial(25, defense(2, 5, 6, 2, 5), 9, net.minecraft.sounds.SoundEvents.ARMOR_EQUIP_TURTLE, 0F, 0F, net.minecraft.tags.ItemTags.REPAIRS_TURTLE_HELMET, EquipmentAssets.TURTLE_SCUTE);
    ArmorMaterial NETHERITE = new ArmorMaterial(37, defense(3, 6, 8, 3, 19), 15, net.minecraft.sounds.SoundEvents.ARMOR_EQUIP_NETHERITE, 3F, 0.1F, net.minecraft.tags.ItemTags.REPAIRS_NETHERITE_ARMOR, EquipmentAssets.NETHERITE);
    ArmorMaterial ARMADILLO_SCUTE = new ArmorMaterial(4, defense(3, 6, 8, 3, 11), 10, net.minecraft.sounds.SoundEvents.ARMOR_EQUIP_WOLF, 0F, 0F, net.minecraft.tags.ItemTags.REPAIRS_WOLF_ARMOR, EquipmentAssets.ARMADILLO_SCUTE);
    private static Map<ArmorType, Integer> defense(int boots, int legs, int chest, int helmet, int body) {
        Map<ArmorType, Integer> map = new java.util.EnumMap<>(ArmorType.class);
        map.put(ArmorType.BOOTS, boots);
        map.put(ArmorType.LEGGINGS, legs);
        map.put(ArmorType.CHESTPLATE, chest);
        map.put(ArmorType.HELMET, helmet);
        map.put(ArmorType.BODY, body);
        return map;
    }
}
