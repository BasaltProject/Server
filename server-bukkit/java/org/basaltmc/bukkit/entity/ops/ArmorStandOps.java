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

    private io.papermc.paper.math.Rotations rotations(String part, EulerAngle fallback) {
        EulerAngle angle = readPose(part, fallback);
        return io.papermc.paper.math.Rotations.ofDegrees(Math.toDegrees(angle.getX()), Math.toDegrees(angle.getY()),
            Math.toDegrees(angle.getZ()));
    }

    private void writeRotations(String part, io.papermc.paper.math.Rotations rotations) {
        writePose(part, new EulerAngle(Math.toRadians(rotations.x()), Math.toRadians(rotations.y()),
            Math.toRadians(rotations.z())));
    }

    default io.papermc.paper.math.Rotations getHeadRotations() {
        return rotations("Head", EulerAngle.ZERO);
    }

    default void setHeadRotations(io.papermc.paper.math.Rotations rotations) {
        writeRotations("Head", rotations);
    }

    default io.papermc.paper.math.Rotations getBodyRotations() {
        return rotations("Body", EulerAngle.ZERO);
    }

    default void setBodyRotations(io.papermc.paper.math.Rotations rotations) {
        writeRotations("Body", rotations);
    }

    default io.papermc.paper.math.Rotations getLeftArmRotations() {
        return rotations("LeftArm", new EulerAngle(Math.toRadians(-10), 0, Math.toRadians(-10)));
    }

    default void setLeftArmRotations(io.papermc.paper.math.Rotations rotations) {
        writeRotations("LeftArm", rotations);
    }

    default io.papermc.paper.math.Rotations getRightArmRotations() {
        return rotations("RightArm", new EulerAngle(Math.toRadians(-15), 0, Math.toRadians(10)));
    }

    default void setRightArmRotations(io.papermc.paper.math.Rotations rotations) {
        writeRotations("RightArm", rotations);
    }

    default io.papermc.paper.math.Rotations getLeftLegRotations() {
        return rotations("LeftLeg", new EulerAngle(Math.toRadians(-1), 0, Math.toRadians(-1)));
    }

    default void setLeftLegRotations(io.papermc.paper.math.Rotations rotations) {
        writeRotations("LeftLeg", rotations);
    }

    default io.papermc.paper.math.Rotations getRightLegRotations() {
        return rotations("RightLeg", new EulerAngle(Math.toRadians(1), 0, Math.toRadians(1)));
    }

    default void setRightLegRotations(io.papermc.paper.math.Rotations rotations) {
        writeRotations("RightLeg", rotations);
    }

    private static int slotBit(EquipmentSlot slot) {
        return switch (slot) {
            case HAND -> 0;
            case OFF_HAND -> 5;
            case FEET -> 1;
            case LEGS -> 2;
            case CHEST -> 3;
            case HEAD -> 4;
            case BODY -> 6;
            case SADDLE -> 7;
        };
    }

    default boolean isSlotDisabled(EquipmentSlot slot) {
        return (core().nbtInt("DisabledSlots") & (1 << slotBit(slot))) != 0
            || slot.isHand() && !hasArms();
    }

    default java.util.Set<EquipmentSlot> getDisabledSlots() {
        java.util.Set<EquipmentSlot> disabled = new java.util.HashSet<>();
        for (EquipmentSlot slot : EquipmentSlot.values()) {
            if (isSlotDisabled(slot)) {
                disabled.add(slot);
            }
        }
        return disabled;
    }

    default void setDisabledSlots(EquipmentSlot... slots) {
        int disabled = 0;
        for (EquipmentSlot slot : slots) {
            if (slot != EquipmentSlot.OFF_HAND) {
                int bit = slotBit(slot);
                disabled += (1 << bit) + (1 << (bit + 8)) + (1 << (bit + 16));
            }
        }
        core().nbtSet("DisabledSlots", "int", disabled);
    }

    default void addDisabledSlots(EquipmentSlot... slots) {
        java.util.Set<EquipmentSlot> disabled = getDisabledSlots();
        java.util.Collections.addAll(disabled, slots);
        setDisabledSlots(disabled.toArray(new EquipmentSlot[0]));
    }

    default void removeDisabledSlots(EquipmentSlot... slots) {
        java.util.Set<EquipmentSlot> disabled = getDisabledSlots();
        for (EquipmentSlot slot : slots) {
            disabled.remove(slot);
        }
        setDisabledSlots(disabled.toArray(new EquipmentSlot[0]));
    }

    private static int lockBit(EquipmentSlot slot, LockType type) {
        return slotBit(slot) + type.ordinal() * 8;
    }

    default void addEquipmentLock(EquipmentSlot slot, LockType type) {
        core().nbtSet("DisabledSlots", "int", core().nbtInt("DisabledSlots") | (1 << lockBit(slot, type)));
    }

    default void removeEquipmentLock(EquipmentSlot slot, LockType type) {
        core().nbtSet("DisabledSlots", "int", core().nbtInt("DisabledSlots") & ~(1 << lockBit(slot, type)));
    }

    default boolean hasEquipmentLock(EquipmentSlot slot, LockType type) {
        return (core().nbtInt("DisabledSlots") & (1 << lockBit(slot, type))) != 0;
    }

    default boolean canMove() {
        return !core().nbtFlag("BasaltNoMove");
    }

    default void setCanMove(boolean move) {
        core().nbtSet("BasaltNoMove", "bool", !move);
    }

    default boolean canTick() {
        return !core().nbtFlag("BasaltNoTick");
    }

    default void setCanTick(boolean tick) {
        core().nbtSet("BasaltNoTick", "bool", !tick);
    }
}
