package org.basaltmc.bukkit.entity.ops;

import org.basaltmc.bukkit.entity.Cored;
import org.bukkit.entity.Zombie;

public interface ZombieOps extends Zombie, Cored {
    default boolean isBaby() {
        return core().integer("breed_age") < 0;
    }

    default void setBaby(boolean baby) {
        core().set("breed_age", baby ? -24000 : 0);
    }

    default int getConversionTime() {
        return core().integer("conversion_countdown");
    }

    default boolean shouldBurnInDay() {
        return core().flag("burns_in_day");
    }

    default void setShouldBurnInDay(boolean burn) {
        core().set("burns_in_day", burn);
    }

    default boolean isArmsRaised() {
        return core().flag("aggressive");
    }

    default void setArmsRaised(boolean raised) {
        core().set("aggressive", raised);
    }

    default boolean isDrowning() {
        return core().integer("conversion_countdown") >= 0;
    }

    default void startDrowning(int conversionTime) {
        core().set("conversion_countdown", conversionTime);
    }

    default void stopDrowning() {
        core().set("conversion_countdown", -1);
    }

    default boolean canBreakDoors() {
        return core().nbtFlag("CanBreakDoors");
    }

    default void setCanBreakDoors(boolean breakDoors) {
        core().nbtSet("CanBreakDoors", "bool", breakDoors);
    }

    default boolean isVillager() {
        return getType() == org.bukkit.entity.EntityType.ZOMBIE_VILLAGER;
    }

    default org.bukkit.entity.Villager.Profession getVillagerProfession() {
        if (!isVillager()) {
            return null;
        }
        java.util.Map<String, Object> data = org.basaltmc.bukkit.item.Nbt.map(EntityNbt.compound(core(), "")
            .get("VillagerData"));
        String id = data == null ? null : org.basaltmc.bukkit.item.Nbt.string(data, "profession", null);
        org.bukkit.entity.Villager.Profession found = id == null ? null : io.papermc.paper.registry.RegistryAccess
            .registryAccess().getRegistry(io.papermc.paper.registry.RegistryKey.VILLAGER_PROFESSION)
            .get(org.bukkit.NamespacedKey.fromString(id));
        return found == null ? org.bukkit.entity.Villager.Profession.NONE : found;
    }

    default boolean isConverting() {
        return core().integer("conversion_countdown") > -1;
    }

    default void setConversionTime(int time) {
        core().set("conversion_countdown", time);
    }
}
