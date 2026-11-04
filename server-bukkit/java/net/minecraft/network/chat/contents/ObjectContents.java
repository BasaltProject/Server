// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.network.chat.contents;
import com.mojang.brigadier.exceptions.CommandSyntaxException;
import com.mojang.serialization.MapCodec;
import com.mojang.serialization.codecs.RecordCodecBuilder;
import java.util.Optional;
import net.minecraft.network.chat.Component;
import net.minecraft.network.chat.ComponentContents;
import net.minecraft.network.chat.ComponentSerialization;
import net.minecraft.network.chat.ComponentUtils;
import net.minecraft.network.chat.FormattedText;
import net.minecraft.network.chat.MutableComponent;
import net.minecraft.network.chat.ResolutionContext;
import net.minecraft.network.chat.Style;
import net.minecraft.network.chat.contents.objects.ObjectInfo;
import net.minecraft.network.chat.contents.objects.ObjectInfos;
public record ObjectContents(ObjectInfo contents, Optional<Component> fallback) implements ComponentContents {
    private static final String kPlaceholder = "￼";
    public static final MapCodec<ObjectContents> MAP_CODEC = RecordCodecBuilder.mapCodec(i -> i.group(
        ObjectInfos.CODEC.forGetter(ObjectContents::contents),
        ComponentSerialization.CODEC.optionalFieldOf("fallback").forGetter(ObjectContents::fallback)).apply(i, ObjectContents::new));
    public MapCodec<ObjectContents> codec() {
        return MAP_CODEC;
    }
    public MutableComponent resolve(ResolutionContext context, int recursionDepth) throws CommandSyntaxException {
        Optional<MutableComponent> resolvedFallback = ComponentUtils.resolve(context, fallback, recursionDepth);
        ObjectInfo validated = context.validate(contents);
        if (validated == null) {
            return resolvedFallback.orElseGet(() -> Component.literal(contents.defaultFallback()));
        }
        return MutableComponent.create(new ObjectContents(validated, resolvedFallback.map(component -> component)));
    }
    public <T> Optional<T> visit(FormattedText.ContentConsumer<T> output) {
        return fallback.isPresent() ? fallback.get().visit(output) : output.accept(contents.defaultFallback());
    }
    public <T> Optional<T> visit(FormattedText.StyledContentConsumer<T> output, Style currentStyle) {
        return output.accept(currentStyle.withFont(contents.fontDescription()), kPlaceholder);
    }
}
