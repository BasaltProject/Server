// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.util;

import java.util.function.Supplier;

public record ModCheck(Confidence confidence, String description) {
    public static ModCheck identify(String expectedBrand, Supplier<String> actualBrand, String component, Class<?> canaryClass) {
        String brand = actualBrand.get();
        if (!expectedBrand.equals(brand)) {
            return new ModCheck(Confidence.DEFINITELY, "Brand changed to '" + brand + "'");
        }
        if (canaryClass.getSigners() == null) {
            return new ModCheck(Confidence.VERY_LIKELY, component + " jar signature invalidated");
        }
        return new ModCheck(Confidence.PROBABLY_NOT, component + " jar signature and brand is untouched");
    }

    public boolean shouldReportAsModified() {
        return confidence.shouldReportAsModified;
    }

    public ModCheck merge(ModCheck other) {
        return new ModCheck((Confidence) java.util.Collections.max(java.util.List.of(confidence, other.confidence)), description + "; " + other.description);
    }

    public String fullDescription() {
        return confidence.description + " " + description;
    }

    public static enum Confidence {
        PROBABLY_NOT("Probably not.", false),
        VERY_LIKELY("Very likely;", true),
        DEFINITELY("Definitely;", true);

        final String description;
        final boolean shouldReportAsModified;

        Confidence(String description, boolean shouldReportAsModified) {
            this.description = description;
            this.shouldReportAsModified = shouldReportAsModified;
        }
    }
}
