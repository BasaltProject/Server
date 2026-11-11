// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.animal.feline;
import net.minecraft.core.registries.Registries;
import net.minecraft.core.Holder;
import net.minecraft.core.RegistryAccess;
import net.minecraft.data.worldgen.BootstrapContext;
import net.minecraft.resources.ResourceKey;
import net.minecraft.util.RandomSource;
public class CatSoundVariants {
    public static final ResourceKey<CatSoundVariant> CLASSIC = ResourceKey.create(Registries.CAT_SOUND_VARIANT, net.minecraft.resources.Identifier.withDefaultNamespace("classic"));
    public static final ResourceKey<CatSoundVariant> ROYAL = ResourceKey.create(Registries.CAT_SOUND_VARIANT, net.minecraft.resources.Identifier.withDefaultNamespace("royal"));
    public static void bootstrap(BootstrapContext<CatSoundVariant> context) {
    }
    public static Holder<CatSoundVariant> pickRandomSoundVariant(RegistryAccess registryAccess, RandomSource random) {
        return registryAccess.lookupOrThrow(Registries.CAT_SOUND_VARIANT).getRandom(random).orElseThrow();
    }
    public static enum SoundSet {
        CLASSIC("classic", "cat"),
        ROYAL("royal", "cat_royal");
        private final String identifier;
        private final String soundEventIdentifier;
        private SoundSet(String identifier, String soundEventIdentifier) {
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
