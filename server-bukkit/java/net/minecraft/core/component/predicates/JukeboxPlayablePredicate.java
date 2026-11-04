// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.core.component.predicates;
import com.mojang.serialization.Codec;
import com.mojang.serialization.codecs.RecordCodecBuilder;
import java.util.Optional;
import net.minecraft.advancements.predicates.SingleComponentItemPredicate;
import net.minecraft.core.Holder;
import net.minecraft.core.HolderSet;
import net.minecraft.core.component.DataComponentType;
import net.minecraft.core.component.DataComponents;
import net.minecraft.resources.ResourceKey;
import net.minecraft.world.item.JukeboxPlayable;
import net.minecraft.world.item.JukeboxSong;
import org.basaltmc.bukkit.facade.ReflectiveCodecs;
public record JukeboxPlayablePredicate(Optional<HolderSet<JukeboxSong>> song) implements SingleComponentItemPredicate<JukeboxPlayable> {
    public static final Codec<JukeboxPlayablePredicate> CODEC = RecordCodecBuilder.create(i -> i.group(
        ReflectiveCodecs.holderSet(JukeboxSong.class).optionalFieldOf("song").forGetter(JukeboxPlayablePredicate::song)).apply(i, JukeboxPlayablePredicate::new));
    public DataComponentType<JukeboxPlayable> componentType() {
        return DataComponents.JUKEBOX_PLAYABLE;
    }
    public boolean matches(JukeboxPlayable value) {
        if (song.isEmpty()) {
            return true;
        }
        for (Holder<JukeboxSong> candidate : song.get()) {
            Optional<ResourceKey<JukeboxSong>> key = candidate.unwrapKey();
            if (!key.isEmpty() && key.equals(value.song().unwrapKey())) {
                return true;
            }
        }
        return false;
    }
    public static JukeboxPlayablePredicate any() {
        return new JukeboxPlayablePredicate(Optional.empty());
    }
}
