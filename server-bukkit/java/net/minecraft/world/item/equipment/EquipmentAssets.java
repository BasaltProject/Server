// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.item.equipment;
import java.util.Map;
import net.minecraft.core.Registry;
import net.minecraft.resources.ResourceKey;
import net.minecraft.world.item.DyeColor;
public interface EquipmentAssets {
    ResourceKey<? extends Registry<EquipmentAsset>> ROOT_ID = ResourceKey.createRegistryKey(net.minecraft.resources.Identifier.withDefaultNamespace("equipment_asset"));
    ResourceKey<EquipmentAsset> LEATHER = createId("leather");
    ResourceKey<EquipmentAsset> COPPER = createId("copper");
    ResourceKey<EquipmentAsset> CHAINMAIL = createId("chainmail");
    ResourceKey<EquipmentAsset> IRON = createId("iron");
    ResourceKey<EquipmentAsset> GOLD = createId("gold");
    ResourceKey<EquipmentAsset> DIAMOND = createId("diamond");
    ResourceKey<EquipmentAsset> TURTLE_SCUTE = createId("turtle_scute");
    ResourceKey<EquipmentAsset> NETHERITE = createId("netherite");
    ResourceKey<EquipmentAsset> ARMADILLO_SCUTE = createId("armadillo_scute");
    ResourceKey<EquipmentAsset> ELYTRA = createId("elytra");
    ResourceKey<EquipmentAsset> SADDLE = createId("saddle");
    Map<DyeColor, ResourceKey<EquipmentAsset>> CARPETS = colored("_carpet");
    ResourceKey<EquipmentAsset> TRADER_LLAMA = createId("trader_llama");
    ResourceKey<EquipmentAsset> TRADER_LLAMA_BABY = createId("trader_llama_baby");
    Map<DyeColor, ResourceKey<EquipmentAsset>> HARNESSES = colored("_harness");
    static ResourceKey<EquipmentAsset> createId(String name) {
        return ResourceKey.create(ROOT_ID, net.minecraft.resources.Identifier.withDefaultNamespace(name));
    }
    private static Map<DyeColor, ResourceKey<EquipmentAsset>> colored(String suffix) {
        Map<DyeColor, ResourceKey<EquipmentAsset>> keys = new java.util.EnumMap<>(DyeColor.class);
        for (DyeColor color : DyeColor.values()) {
            keys.put(color, createId(color.getSerializedName() + suffix));
        }
        return keys;
    }
}
