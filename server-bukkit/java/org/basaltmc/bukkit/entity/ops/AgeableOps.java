package org.basaltmc.bukkit.entity.ops;

import org.bukkit.entity.Ageable;

public interface AgeableOps extends Ageable, org.basaltmc.bukkit.entity.Cored {
    int kBabyAge = -24000;

    default int getAge() {
        return core().integer("breed_age");
    }

    default void setAge(int age) {
        core().set("breed_age", age);
    }

    default boolean isAdult() {
        return getAge() >= 0;
    }

    default void setAdult() {
        setAge(0);
    }

    default void setBaby() {
        setAge(kBabyAge);
    }

    default boolean canBreed() {
        return getAge() == 0;
    }

    default void setBreed(boolean breed) {
        setAge(breed ? 0 : 6000);
    }
}
