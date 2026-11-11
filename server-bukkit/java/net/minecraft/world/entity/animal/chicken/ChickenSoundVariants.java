// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.animal.chicken;
import net.minecraft.core.registries.Registries;
import net.minecraft.core.Holder;
import net.minecraft.core.RegistryAccess;
import net.minecraft.data.worldgen.BootstrapContext;
import net.minecraft.resources.ResourceKey;
import net.minecraft.util.RandomSource;
public class ChickenSoundVariants {
    public static final ResourceKey<ChickenSoundVariant> CLASSIC = ResourceKey.create(Registries.CHICKEN_SOUND_VARIANT, net.minecraft.resources.Identifier.withDefaultNamespace("classic"));
    public static final ResourceKey<ChickenSoundVariant> PICKY = ResourceKey.create(Registries.CHICKEN_SOUND_VARIANT, net.minecraft.resources.Identifier.withDefaultNamespace("picky"));
    public static void bootstrap(BootstrapContext<ChickenSoundVariant> context) {
    }
    public static Holder<ChickenSoundVariant> pickRandomSoundVariant(RegistryAccess registryAccess, RandomSource random) {
        return registryAccess.lookupOrThrow(Registries.CHICKEN_SOUND_VARIANT).getRandom(random).orElseThrow();
    }
    public static enum SoundSet {
        CLASSIC("classic", "chicken"),
        PICKY("picky", "chicken_picky");
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
