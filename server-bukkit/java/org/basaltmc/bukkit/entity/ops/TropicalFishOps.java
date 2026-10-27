package org.basaltmc.bukkit.entity.ops;

import org.bukkit.DyeColor;
import org.basaltmc.bukkit.entity.Cored;
import org.bukkit.entity.TropicalFish;

public interface TropicalFishOps extends TropicalFish, Cored {
    TropicalFish.Pattern[] kPatterns = {TropicalFish.Pattern.KOB, TropicalFish.Pattern.SUNSTREAK,
        TropicalFish.Pattern.SNOOPER, TropicalFish.Pattern.DASHER, TropicalFish.Pattern.BRINELY,
        TropicalFish.Pattern.SPOTTY, TropicalFish.Pattern.FLOPPER, TropicalFish.Pattern.STRIPEY,
        TropicalFish.Pattern.GLITTER, TropicalFish.Pattern.BLOCKFISH, TropicalFish.Pattern.BETTY,
        TropicalFish.Pattern.CLAYFISH};

    default int variant() {
        return core().nbtInt("Variant");
    }

    default DyeColor getBodyColor() {
        return DyeColor.getByWoolData((byte) ((variant() >> 16) & 255));
    }

    default void setBodyColor(DyeColor color) {
        core().nbtSet("Variant", "int", (variant() & ~(255 << 16)) | (color.getWoolData() << 16));
    }

    default DyeColor getPatternColor() {
        return DyeColor.getByWoolData((byte) ((variant() >> 24) & 255));
    }

    default void setPatternColor(DyeColor color) {
        core().nbtSet("Variant", "int", (variant() & 0x00FFFFFF) | (color.getWoolData() << 24));
    }

    default TropicalFish.Pattern getPattern() {
        int base = variant() & 255;
        int index = (variant() >> 8) & 255;
        int at = Math.min(5, Math.max(0, index)) + (base == 0 ? 0 : 6);
        return kPatterns[at];
    }

    default void setPattern(TropicalFish.Pattern pattern) {
        int at = java.util.Arrays.asList(kPatterns).indexOf(java.util.Objects.requireNonNull(pattern, "pattern"));
        core().nbtSet("Variant", "int", (variant() & 0xFFFF0000) | ((at % 6) << 8) | (at >= 6 ? 1 : 0));
    }
}
