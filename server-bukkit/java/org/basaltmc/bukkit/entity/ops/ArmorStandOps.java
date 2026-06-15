package org.basaltmc.bukkit.entity.ops;

import net.minecraft.nbt.CompoundTag;
import net.minecraft.nbt.FloatTag;
import net.minecraft.nbt.ListTag;
import org.basaltmc.bukkit.entity.Cored;
import org.basaltmc.bukkit.facade.NbtCodec;
import org.bukkit.entity.ArmorStand;
import org.bukkit.inventory.EquipmentSlot;
import org.bukkit.inventory.ItemStack;
import org.bukkit.util.EulerAngle;

public interface ArmorStandOps extends ArmorStand, Cored {
    private EulerAngle readPose(String part, EulerAngle fallback) {
        byte[] stored = core().nbt("Pose");
        CompoundTag pose = stored == null ? null : NbtCodec.fromBytes(stored);
        if (pose == null || !pose.contains(part)) {
            return fallback;
        }
        ListTag angles = pose.getListOrEmpty(part);
        return new EulerAngle(Math.toRadians(angles.getFloatOr(0, 0f)), Math.toRadians(angles.getFloatOr(1, 0f)),
            Math.toRadians(angles.getFloatOr(2, 0f)));
    }

    private void writePose(String part, EulerAngle angle) {
        ListTag angles = new ListTag();
        angles.add(FloatTag.valueOf((float) Math.toDegrees(angle.getX())));
        angles.add(FloatTag.valueOf((float) Math.toDegrees(angle.getY())));
        angles.add(FloatTag.valueOf((float) Math.toDegrees(angle.getZ())));
        CompoundTag pose = new CompoundTag();
        pose.put(part, angles);
        CompoundTag patch = new CompoundTag();
        patch.put("Pose", pose);
        core().mergeNbt(NbtCodec.toBytes(patch));
    }

    default EulerAngle getHeadPose() {
        return readPose("Head", EulerAngle.ZERO);
    }

    default void setHeadPose(EulerAngle angle) {
        writePose("Head", angle);
    }

    default EulerAngle getBodyPose() {
        return readPose("Body", EulerAngle.ZERO);
    }

    default void setBodyPose(EulerAngle angle) {
        writePose("Body", angle);
    }

    default EulerAngle getLeftArmPose() {
        return readPose("LeftArm", new EulerAngle(Math.toRadians(-10), 0, Math.toRadians(-10)));
    }

    default void setLeftArmPose(EulerAngle angle) {
        writePose("LeftArm", angle);
    }

    default EulerAngle getRightArmPose() {
        return readPose("RightArm", new EulerAngle(Math.toRadians(-15), 0, Math.toRadians(10)));
    }

    default void setRightArmPose(EulerAngle angle) {
        writePose("RightArm", angle);
    }

    default EulerAngle getLeftLegPose() {
        return readPose("LeftLeg", new EulerAngle(Math.toRadians(-1), 0, Math.toRadians(-1)));
    }

    default void setLeftLegPose(EulerAngle angle) {
        writePose("LeftLeg", angle);
    }

    default EulerAngle getRightLegPose() {
        return readPose("RightLeg", new EulerAngle(Math.toRadians(1), 0, Math.toRadians(1)));
    }

    default void setRightLegPose(EulerAngle angle) {
        writePose("RightLeg", angle);
    }

    default boolean isSmall() {
        return core().nbtFlag("Small");
    }

    default void setSmall(boolean small) {
        core().nbtSet("Small", "bool", small);
    }

    default boolean hasArms() {
        return core().nbtFlag("ShowArms");
    }

    default void setArms(boolean arms) {
        core().nbtSet("ShowArms", "bool", arms);
    }

    default boolean hasBasePlate() {
        return !core().nbtFlag("NoBasePlate");
    }

    default void setBasePlate(boolean basePlate) {
        core().nbtSet("NoBasePlate", "bool", !basePlate);
    }

    default boolean isMarker() {
        return core().nbtFlag("Marker");
    }

    default void setMarker(boolean marker) {
        core().nbtSet("Marker", "bool", marker);
    }

    default boolean isVisible() {
        return !core().flag("invisible");
    }

    default void setVisible(boolean visible) {
        core().set("invisible", !visible);
    }

    default ItemStack getItem(EquipmentSlot slot) {
        return getEquipment().getItem(slot);
    }

    default void setItem(EquipmentSlot slot, ItemStack item) {
        getEquipment().setItem(slot, item);
    }

    default ItemStack getItemInHand() {
        return getEquipment().getItemInMainHand();
    }

    default void setItemInHand(ItemStack item) {
        getEquipment().setItemInMainHand(item);
    }

    default ItemStack getBoots() {
        return getEquipment().getBoots();
    }

    default void setBoots(ItemStack item) {
        getEquipment().setBoots(item);
    }

    default ItemStack getLeggings() {
        return getEquipment().getLeggings();
    }

    default void setLeggings(ItemStack item) {
        getEquipment().setLeggings(item);
    }

    default ItemStack getChestplate() {
        return getEquipment().getChestplate();
    }

    default void setChestplate(ItemStack item) {
        getEquipment().setChestplate(item);
    }

    default ItemStack getHelmet() {
        return getEquipment().getHelmet();
    }

    default void setHelmet(ItemStack item) {
        getEquipment().setHelmet(item);
    }
}
