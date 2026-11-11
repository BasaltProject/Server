// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.animal.wolf;
import net.minecraft.core.registries.Registries;
import net.minecraft.core.Holder;
import net.minecraft.core.RegistryAccess;
import net.minecraft.data.worldgen.BootstrapContext;
import net.minecraft.resources.ResourceKey;
import net.minecraft.util.RandomSource;
public class WolfSoundVariants {
    public static final ResourceKey<WolfSoundVariant> CLASSIC = ResourceKey.create(Registries.WOLF_SOUND_VARIANT, net.minecraft.resources.Identifier.withDefaultNamespace("classic"));
    public static final ResourceKey<WolfSoundVariant> PUGLIN = ResourceKey.create(Registries.WOLF_SOUND_VARIANT, net.minecraft.resources.Identifier.withDefaultNamespace("puglin"));
    public static final ResourceKey<WolfSoundVariant> SAD = ResourceKey.create(Registries.WOLF_SOUND_VARIANT, net.minecraft.resources.Identifier.withDefaultNamespace("sad"));
    public static final ResourceKey<WolfSoundVariant> ANGRY = ResourceKey.create(Registries.WOLF_SOUND_VARIANT, net.minecraft.resources.Identifier.withDefaultNamespace("angry"));
    public static final ResourceKey<WolfSoundVariant> GRUMPY = ResourceKey.create(Registries.WOLF_SOUND_VARIANT, net.minecraft.resources.Identifier.withDefaultNamespace("grumpy"));
    public static final ResourceKey<WolfSoundVariant> BIG = ResourceKey.create(Registries.WOLF_SOUND_VARIANT, net.minecraft.resources.Identifier.withDefaultNamespace("big"));
    public static final ResourceKey<WolfSoundVariant> CUTE = ResourceKey.create(Registries.WOLF_SOUND_VARIANT, net.minecraft.resources.Identifier.withDefaultNamespace("cute"));
    public static void bootstrap(BootstrapContext<WolfSoundVariant> context) {
    }
    public static Holder<WolfSoundVariant> pickRandomSoundVariant(RegistryAccess registryAccess, RandomSource random) {
        return registryAccess.lookupOrThrow(Registries.WOLF_SOUND_VARIANT).getRandom(random).orElseThrow();
    }
    public static enum SoundSet {
        CLASSIC("classic", "wolf"),
        PUGLIN("puglin", "wolf_puglin"),
        SAD("sad", "wolf_sad"),
        ANGRY("angry", "wolf_angry"),
        GRUMPY("grumpy", "wolf_grumpy"),
        BIG("big", "wolf_big"),
        CUTE("cute", "wolf_cute");
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
