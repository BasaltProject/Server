package org.basaltmc.bukkit.entity.ops;

import io.papermc.paper.entity.Bucketable;
import org.basaltmc.bukkit.entity.Cored;
import org.bukkit.Material;
import org.bukkit.NamespacedKey;
import org.bukkit.Registry;
import org.bukkit.Sound;
import org.bukkit.inventory.ItemStack;

public interface BucketableOps extends Bucketable, Cored {
    default boolean isFromBucket() {
        return core().nbtFlag("FromBucket");
    }

    default void setFromBucket(boolean fromBucket) {
        core().nbtSet("FromBucket", "bool", fromBucket);
    }

    default ItemStack getBaseBucketItem() {
        Material bucket = Material.matchMaterial(core().keyName() + "_bucket");
        return new ItemStack(bucket != null ? bucket : Material.WATER_BUCKET);
    }

    default Sound getPickupSound() {
        String kind = switch (core().keyName()) {
            case "axolotl" -> "axolotl";
            case "tadpole" -> "tadpole";
            default -> "fish";
        };
        return Registry.SOUNDS.get(NamespacedKey.minecraft("item.bucket.fill_" + kind));
    }
}
