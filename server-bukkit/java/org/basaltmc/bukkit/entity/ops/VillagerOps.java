package org.basaltmc.bukkit.entity.ops;

import org.basaltmc.bukkit.entity.Cored;
import org.bukkit.NamespacedKey;
import org.bukkit.Registry;
import org.bukkit.entity.Villager;

public interface VillagerOps extends Villager, Cored {
    default Villager.Profession getProfession() {
        String name = core().text("profession");
        Villager.Profession found = name == null ? null : Registry.VILLAGER_PROFESSION.get(NamespacedKey.minecraft(name));
        return found == null ? Villager.Profession.NONE : found;
    }

    default void setProfession(Villager.Profession profession) {
        core().set("profession", profession.getKey().getKey());
    }

    default int getVillagerLevel() {
        return core().integer("villager_level");
    }

    default void setVillagerLevel(int level) {
        if (level < 1 || level > 5) {
            throw new IllegalArgumentException("Level must be between 1 and 5");
        }
        core().set("villager_level", level);
    }

    default int getVillagerExperience() {
        return core().integer("villager_experience");
    }

    default void setVillagerExperience(int experience) {
        core().set("villager_experience", Math.max(0, experience));
    }

    default Villager.Type getVillagerType() {
        String name = core().nbtText("VillagerData.type");
        Villager.Type found = name == null ? null : Registry.VILLAGER_TYPE.get(NamespacedKey.fromString(name));
        return found == null ? Villager.Type.PLAINS : found;
    }

    default void setVillagerType(Villager.Type type) {
        core().nbtSet("VillagerData.type", "string", type.getKey().toString());
    }
}
