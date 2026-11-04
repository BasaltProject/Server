// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.monster.warden;

import java.util.Arrays;
import net.minecraft.sounds.SoundEvent;

public enum AngerLevel {
    CALM(0), AGITATED(40), ANGRY(80);

    private static final AngerLevel[] kSortedLevels = sorted();

    private final int minimumAnger;

    AngerLevel(int minimumAnger) {
        this.minimumAnger = minimumAnger;
    }

    private static AngerLevel[] sorted() {
        AngerLevel[] values = values();
        Arrays.sort(values, (a, b) -> Integer.compare(b.minimumAnger, a.minimumAnger));
        return values;
    }

    public int getMinimumAnger() {
        return minimumAnger;
    }

    public SoundEvent getAmbientSound() {
        return null;
    }

    public SoundEvent getListeningSound() {
        return null;
    }

    public static AngerLevel byAnger(int anger) {
        for (AngerLevel level : kSortedLevels) {
            if (anger >= level.minimumAnger) {
                return level;
            }
        }
        return CALM;
    }

    public boolean isAngry() {
        return this == ANGRY;
    }
}
