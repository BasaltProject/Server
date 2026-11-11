// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.server.dialog.input;
import com.mojang.serialization.Codec;
import com.mojang.serialization.DataResult;
import com.mojang.serialization.MapCodec;
import com.mojang.serialization.codecs.RecordCodecBuilder;
import java.util.List;
import java.util.Optional;
import net.minecraft.network.chat.Component;
import net.minecraft.network.chat.ComponentSerialization;
import net.minecraft.server.dialog.Dialog;
import net.minecraft.util.ExtraCodecs;
public record SingleOptionInput(int width, List<Entry> entries, Component label, boolean labelVisible) implements InputControl {
    public static final MapCodec<SingleOptionInput> MAP_CODEC = RecordCodecBuilder.<SingleOptionInput>mapCodec(i -> i.group(
            Dialog.WIDTH_CODEC.optionalFieldOf("width", 200).forGetter(SingleOptionInput::width),
            ExtraCodecs.nonEmptyList(Entry.CODEC.listOf()).fieldOf("options").forGetter(SingleOptionInput::entries),
            ComponentSerialization.CODEC.fieldOf("label").forGetter(SingleOptionInput::label),
            Codec.BOOL.optionalFieldOf("label_visible", true).forGetter(SingleOptionInput::labelVisible))
            .apply(i, SingleOptionInput::new))
            .validate(input -> input.entries.stream().filter(Entry::initial).count() > 1
                    ? DataResult.error(() -> "Multiple initial values")
                    : DataResult.success(input));
    public MapCodec<SingleOptionInput> mapCodec() {
        return MAP_CODEC;
    }
    public Optional<Entry> initial() {
        return entries.stream().filter(Entry::initial).findFirst();
    }
    public static record Entry(String id, Optional<Component> display, boolean initial) {
        public static final Codec<Entry> FULL_CODEC = RecordCodecBuilder.create(i -> i.group(
                Codec.STRING.fieldOf("id").forGetter(Entry::id),
                ComponentSerialization.CODEC.optionalFieldOf("display").forGetter(Entry::display),
                Codec.BOOL.optionalFieldOf("initial", false).forGetter(Entry::initial))
                .apply(i, Entry::new));
        public static final Codec<Entry> CODEC = Codec.withAlternative(FULL_CODEC, Codec.STRING, id -> new Entry(id, Optional.empty(), false));
        public Component displayOrDefault() {
            return display.orElseGet(() -> Component.literal(id));
        }
    }
}
