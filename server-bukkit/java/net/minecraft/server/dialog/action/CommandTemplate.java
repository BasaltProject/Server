// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.server.dialog.action;
import com.mojang.serialization.MapCodec;
import com.mojang.serialization.codecs.RecordCodecBuilder;
import java.util.Map;
import java.util.Optional;
import net.minecraft.network.chat.ClickEvent;
public record CommandTemplate(ParsedTemplate template) implements Action {
    public static final MapCodec<CommandTemplate> MAP_CODEC = RecordCodecBuilder.mapCodec(i -> i.group(
            ParsedTemplate.CODEC.fieldOf("template").forGetter(CommandTemplate::template))
            .apply(i, CommandTemplate::new));
    public MapCodec<CommandTemplate> codec() {
        return MAP_CODEC;
    }
    public Optional<ClickEvent> createAction(Map<String, ValueGetter> parameters) {
        return Optional.of(new ClickEvent.RunCommand(template.instantiate(ValueGetter.getAsTemplateSubstitutions(parameters))));
    }
}
