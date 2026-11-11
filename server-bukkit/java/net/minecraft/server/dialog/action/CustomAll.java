// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.server.dialog.action;
import com.mojang.serialization.MapCodec;
import com.mojang.serialization.codecs.RecordCodecBuilder;
import java.util.Map;
import java.util.Optional;
import net.minecraft.nbt.CompoundTag;
import net.minecraft.network.chat.ClickEvent;
import net.minecraft.resources.Identifier;
public record CustomAll(Identifier id, Optional<CompoundTag> additions) implements Action {
    public static final MapCodec<CustomAll> MAP_CODEC = RecordCodecBuilder.mapCodec(i -> i.group(
            Identifier.CODEC.fieldOf("id").forGetter(CustomAll::id),
            CompoundTag.CODEC.optionalFieldOf("additions").forGetter(CustomAll::additions))
            .apply(i, CustomAll::new));
    public MapCodec<CustomAll> codec() {
        return MAP_CODEC;
    }
    public Optional<ClickEvent> createAction(Map<String, ValueGetter> parameters) {
        CompoundTag payload = additions.map(CompoundTag::copy).orElseGet(CompoundTag::new);
        parameters.forEach((key, getter) -> payload.put(key, getter.asTag()));
        return Optional.of(new ClickEvent.Custom(id, Optional.of(payload)));
    }
}
