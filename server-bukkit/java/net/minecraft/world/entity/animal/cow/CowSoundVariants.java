// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.animal.cow;

import net.minecraft.core.Holder;
import net.minecraft.core.RegistryAccess;
import net.minecraft.core.registries.Registries;
import net.minecraft.data.worldgen.BootstrapContext;
import net.minecraft.resources.Identifier;
import net.minecraft.resources.ResourceKey;
import net.minecraft.sounds.SoundEvents;
import net.minecraft.util.RandomSource;

public class CowSoundVariants {
    public static final ResourceKey<CowSoundVariant> CLASSIC = keyOf(SoundSet.CLASSIC);
    public static final ResourceKey<CowSoundVariant> MOODY = keyOf(SoundSet.MOODY);

    private static ResourceKey<CowSoundVariant> keyOf(SoundSet set) {
        return ResourceKey.create(Registries.COW_SOUND_VARIANT, Identifier.withDefaultNamespace(set.getIdentifier()));
    }

    public static void bootstrap(BootstrapContext<CowSoundVariant> context) {
        context.register(CLASSIC, SoundEvents.COW_SOUNDS.get(SoundSet.CLASSIC));
        context.register(MOODY, SoundEvents.COW_SOUNDS.get(SoundSet.MOODY));
    }

    public static Holder<CowSoundVariant> pickRandomSoundVariant(RegistryAccess registryAccess, RandomSource random) {
        return registryAccess.lookupOrThrow(Registries.COW_SOUND_VARIANT).getRandom(random).orElseThrow();
    }

    public static enum SoundSet {
        CLASSIC("classic", "cow"),
        MOODY("moody", "cow_moody");

        private final String identifier;
        private final String soundEventIdentifier;

        SoundSet(String identifier, String soundEventIdentifier) {
            this.identifier = identifier;
            this.soundEventIdentifier = soundEventIdentifier;
        }

        public String getIdentifier() {
            return identifier;
        }

        public String getSoundEventIdentifier() {
            return soundEventIdentifier;
        }
    }
}
