// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.item;
import net.minecraft.core.Holder;
import net.minecraft.data.worldgen.BootstrapContext;
import net.minecraft.resources.ResourceKey;
import net.minecraft.sounds.SoundEvent;
public interface Instruments {
    @SuppressWarnings({"unchecked", "rawtypes"})
    private static <T> ResourceKey<T> key(String name) {
        return (ResourceKey) ResourceKey.create(net.minecraft.core.registries.Registries.INSTRUMENT,
            net.minecraft.resources.Identifier.withDefaultNamespace(name));
    }

    int GOAT_HORN_RANGE_BLOCKS = 256;
    float GOAT_HORN_DURATION = 7.0F;
    int GOAT_HORN_INSTRUMENT_DAMAGE = 0;
    ResourceKey<Instrument> PONDER_GOAT_HORN = key("ponder_goat_horn");
    ResourceKey<Instrument> SING_GOAT_HORN = key("sing_goat_horn");
    ResourceKey<Instrument> SEEK_GOAT_HORN = key("seek_goat_horn");
    ResourceKey<Instrument> FEEL_GOAT_HORN = key("feel_goat_horn");
    ResourceKey<Instrument> ADMIRE_GOAT_HORN = key("admire_goat_horn");
    ResourceKey<Instrument> CALL_GOAT_HORN = key("call_goat_horn");
    ResourceKey<Instrument> YEARN_GOAT_HORN = key("yearn_goat_horn");
    ResourceKey<Instrument> DREAM_GOAT_HORN = key("dream_goat_horn");
    static void bootstrap(BootstrapContext<Instrument> context) {
        ResourceKey<Instrument>[] keys = new ResourceKey[] {PONDER_GOAT_HORN, SING_GOAT_HORN, SEEK_GOAT_HORN, FEEL_GOAT_HORN,
            ADMIRE_GOAT_HORN, CALL_GOAT_HORN, YEARN_GOAT_HORN, DREAM_GOAT_HORN};
        for (int index = 0; index < keys.length; index++) {
            register(context, keys[index], net.minecraft.sounds.SoundEvents.GOAT_HORN_SOUND_VARIANTS.get(index), GOAT_HORN_DURATION,
                GOAT_HORN_RANGE_BLOCKS, GOAT_HORN_INSTRUMENT_DAMAGE);
        }
    }
    static void register(BootstrapContext<Instrument> context, ResourceKey<Instrument> key, Holder<SoundEvent> soundEvent, float duration, float range, int durabilityDamage) {
        context.register(key, new Instrument(soundEvent, duration, range, durabilityDamage,
            net.minecraft.network.chat.Component.translatable(net.minecraft.util.Util.makeDescriptionId("instrument", key.identifier()))));
    }
}
