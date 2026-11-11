// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.animal.pig;
import net.minecraft.core.registries.Registries;
import net.minecraft.core.Holder;
import net.minecraft.core.RegistryAccess;
import net.minecraft.data.worldgen.BootstrapContext;
import net.minecraft.resources.ResourceKey;
import net.minecraft.util.RandomSource;
public class PigSoundVariants {
    public static final ResourceKey<PigSoundVariant> CLASSIC = ResourceKey.create(Registries.PIG_SOUND_VARIANT, net.minecraft.resources.Identifier.withDefaultNamespace("classic"));
    public static final ResourceKey<PigSoundVariant> MINI = ResourceKey.create(Registries.PIG_SOUND_VARIANT, net.minecraft.resources.Identifier.withDefaultNamespace("mini"));
    public static final ResourceKey<PigSoundVariant> BIG = ResourceKey.create(Registries.PIG_SOUND_VARIANT, net.minecraft.resources.Identifier.withDefaultNamespace("big"));
    public static void bootstrap(BootstrapContext<PigSoundVariant> context) {
    }
    public static Holder<PigSoundVariant> pickRandomSoundVariant(RegistryAccess registryAccess, RandomSource random) {
        return registryAccess.lookupOrThrow(Registries.PIG_SOUND_VARIANT).getRandom(random).orElseThrow();
    }
    public static enum SoundSet {
        CLASSIC("classic", "pig"),
        MINI("mini", "pig_mini"),
        BIG("big", "pig_big");
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
